#!/usr/bin/env python3
"""Run the existing cyclic-battle bot and retain raw measurements (Linux /proc).
Creates NEW MySQL schemas with a measure_ prefix; never drops existing schemas.
Use a dedicated MySQL/Redis instance. No server scheduling/worker changes.
"""
import argparse
import collections
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import resource
import socket
import subprocess
import tempfile
import time


def read_config(path):
    sections = {}
    section = None
    for line in Path(path).read_text().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if not line.startswith(" ") and line.endswith(":"):
            section = line[:-1]
            sections[section] = {}
        else:
            key, value = line.strip().split(":", 1)
            sections[section][key] = value.strip().strip('"')
    return sections


def write_config(path, config):
    Path(path).write_text("".join(
        section + ":\n" + "".join(f'  {key}: "{value}"\n' for key, value in values.items())
        for section, values in config.items()))


def command_output(args):
    return subprocess.run(args, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, check=False).stdout.strip()


def proc_sample(pid):
    try:
        base = Path(f"/proc/{pid}")
        fields = (base / "stat").read_text().rsplit(")", 1)[1].split()
        status = dict(line.split(":", 1) for line in (base / "status").read_text().splitlines())
        return {"cpu_ticks": int(fields[11]) + int(fields[12]),
                "rss_kib": int(status.get("VmRSS", "0 kB").split()[0]),
                "threads": int(status["Threads"]),
                "fds": len(list((base / "fd").iterdir()))}
    except (FileNotFoundError, ProcessLookupError, PermissionError):
        # /proc can revoke access while the process exits, even before State becomes Z.
        # Retain a missing sample (None), never manufacture a zero resource reading.
        return None


def histogram_summary(items):
    buckets = collections.Counter()
    count = total = maximum = 0
    for item in items:
        count += item["count"]
        total += item["sum_us"]
        maximum = max(maximum, item["max_us"])
        buckets.update(dict(item["buckets"]))
    def percentile(q):
        rank = max(1, math.ceil(count * q))
        seen = 0
        for index, n in sorted(buckets.items()):
            seen += n
            if seen >= rank:
                return maximum if index == 255 else min(maximum, 1.1 ** index)
        return None
    return {"count": count, "mean_us": total / count if count else None,
            "max_us": maximum if count else None, "p50_us": percentile(.5),
            "p95_us": percentile(.95), "p99_us": percentile(.99)}


def summarize_metrics(path, start_ms, end_ms):
    rows = [json.loads(line) for line in Path(path).read_text().splitlines() if line.strip()]
    selected = [row for row in rows if start_ms <= row["unix_ms"] <= end_ms]
    if len(selected) < 2:
        raise ValueError(f"insufficient measurement windows: {path}")
    first, last = selected[0], selected[-1]
    seconds = last["elapsed_s"] - first["elapsed_s"]
    counters = {k: last["counters"][k] - first["counters"][k] for k in last["counters"]}
    gauges = {}
    for key in last["gauges"]:
        values = [r["gauges"][key] for r in selected]
        gauges[key] = {"min": min(values), "max": max(values), "mean": sum(values) / len(values),
                       "first": values[0], "last": values[-1], "delta": values[-1] - values[0]}
    histograms = {key: histogram_summary([r["histograms"][key] for r in selected[1:]])
                  for key in last["histograms"]}
    return {"actual_seconds": seconds, "samples": len(selected), "counters_delta": counters,
            "rates_per_second": {k: v / seconds for k, v in counters.items()},
            "gauges": gauges, "histograms": histograms, "last_export": rows[-1],
            "all_run_histograms": {key: histogram_summary([r["histograms"][key] for r in rows])
                                   for key in last["histograms"]}}


def audit_bot(gauges):
    """Evaluate conservation only on the final export after all workers drain."""
    recovery = sum(gauges[f"recovery_{name}_total"] for name in
                   ("state", "no_room", "rejected", "timeout", "cancelled", "terminal", "invalid"))
    return {
        "input_accounted": gauges["input_enqueued_total"] == sum(gauges[k] for k in
            ("input_written_total", "input_cancelled_total", "input_dropped_total", "input_pending")),
        "heartbeat_accounted": gauges["heartbeat_sent_total"] == sum(gauges[k] for k in
            ("heartbeat_responses_total", "heartbeat_timeouts_total", "heartbeat_cancelled_total", "heartbeat_pending")),
        "recovery_accounted": gauges["reconnect_attempts_total"] == recovery + gauges["recovery_pending"],
        "workers_drained": gauges["io_workers_live"] == 0,
        "connections_drained": gauges["connections_live"] == 0,
        "pending_drained": all(gauges[k] == 0 for k in
                               ("input_pending", "heartbeat_pending", "recovery_pending")),
        "no_worker_exceptions": gauges["worker_exceptions_total"] == 0,
    }


def version_record(revision, paths):
    """Fail rather than silently label a dirty tool/server with an unrelated revision."""
    revision = subprocess.check_output(["git", "rev-parse", "--verify", revision + "^{commit}"], text=True).strip()
    subprocess.run(["git", "diff", "--exit-code", revision, "--", *paths], check=True,
                   stdout=subprocess.DEVNULL)
    return {"commit": revision, "source_objects": {
        path: subprocess.check_output(["git", "rev-parse", f"{revision}:{path}"], text=True).strip()
        for path in paths}}


def summarize(run_dir):
    meta = json.loads((run_dir / "metadata.json").read_text())
    start, end = meta["sample_start_unix_ms"], meta["sample_end_unix_ms"]
    result = {"bots": meta["bots"], "run": run_dir.name,
              "server": summarize_metrics(run_dir / "server.jsonl", start, end),
              "bot": summarize_metrics(run_dir / "bot.jsonl", start, end)}
    rows = [json.loads(line) for line in (run_dir / "resources.jsonl").read_text().splitlines()]
    rows = [r for r in rows if start <= r["unix_ms"] <= end]
    processes = collections.defaultdict(list)
    for row in rows:
        for name, value in row["processes"].items():
            if value:
                processes[name].append(value)
    result["resources"] = {}
    for name, values in processes.items():
        result["resources"][name] = {
            key: {"min": min(v[key] for v in values), "mean": sum(v[key] for v in values) / len(values),
                  "max": max(v[key] for v in values)}
            for key in ("cpu_percent", "rss_kib", "threads", "fds")}
        result["resources"][name]["samples"] = len(values)
        result["resources"][name]["unavailable_samples"] = len(rows) - len(values)
    if "io_workers_live" in result["bot"]["last_export"]["gauges"]:
        result["audit"] = audit_bot(result["bot"]["last_export"]["gauges"])
        configured = meta.get("bot_threads", result["bot"]["gauges"]["io_threads_configured"]["last"])
        result["audit"]["fixed_workers_in_window"] = all(
            result["bot"]["gauges"]["io_workers_live"][k] == configured for k in ("min", "max"))
        result["audit"]["fixed_process_threads"] = all(
            result["resources"]["bot"]["threads"][k] == configured + 2 for k in ("min", "max"))
        result["audit"]["clean_exit"] = meta["bot_exit_code"] == 0
        result["versions"] = meta.get("versions")
    (run_dir / "summary.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def run_one(args, root, config, index, count):
    run_dir = root / f"{index:02d}-{count}-bots"
    run_dir.mkdir()
    config = {k: dict(v) for k, v in config.items()}
    schema = f"measure_{datetime.datetime.now().strftime('%Y%m%d%H%M%S')}_{index}"
    config["mysql"]["database"] = schema
    mysql = config["mysql"]
    # Defaults file keeps credentials out of process argv and committed artifacts.
    with tempfile.TemporaryDirectory(prefix="game-measure-auth-") as auth_dir:
        auth = Path(auth_dir) / "mysql.cnf"
        auth.write_text("[client]\n" + "\n".join(
            f"{key}={mysql[key]}" for key in ("host", "port", "user", "password")) + "\n")
        auth.chmod(0o600)
        sql = (Path(__file__).resolve().parents[1] / "config/mysql.sql").read_text()
        sql = "\n".join(line for line in sql.splitlines()
                        if not line.startswith(("CREATE DATABASE", "USE ")))
        subprocess.run([args.mysql, f"--defaults-extra-file={auth}"],
                       input=f"CREATE DATABASE {schema}; USE {schema};\n" + sql,
                       text=True, check=True)
        run_config = Path(auth_dir) / "server.yaml"
        write_config(run_config, config)
        env = os.environ.copy()
        env["GAME_SERVER_CONFIG"] = str(run_config)
        env["GAME_METRICS_FILE"] = str(run_dir / "server.jsonl")
        server = bot = None
        meta = {"bots": count, "bot_threads": args.bot_threads, "seed": args.seed, "versions": args.versions, "warmup_seconds": args.warmup, "sample_seconds_requested": args.seconds,
                "config": {k: {a: ("<redacted>" if a == "password" else b) for a, b in v.items()}
                           for k, v in config.items()}}
        with (run_dir / "server.log").open("w") as server_log, (run_dir / "bot.log").open("w") as bot_log:
            try:
                server = subprocess.Popen([str(args.server)], env=env, stdout=server_log,
                                          stderr=subprocess.STDOUT)
                for _ in range(100):
                    if server.poll() is not None:
                        raise RuntimeError("server exited during startup; see server.log")
                    try:
                        with socket.create_connection(("127.0.0.1", int(config["server"]["port"])), timeout=.1):
                            break
                    except OSError:
                        time.sleep(.1)
                else:
                    raise RuntimeError("server listen timeout")
                command = [str(args.bot), "--host", "127.0.0.1", "--port", config["server"]["port"],
                           "--count", str(count), "--duration", str(args.warmup + args.seconds + 5),
                           "--metrics-file", str(run_dir / "bot.jsonl"),
                           "--recovery-file", str(run_dir / "recovery.jsonl"),
                           "--io-threads", str(args.bot_threads), "--seed", str(args.seed),
                           "--account-prefix", f"measure_{index}_",
                           "--cancel-match-percent", "0", "--queue-disconnect-percent", "0",
                           "--room-disconnect-percent", "0", "--playing-disconnect-percent", "0",
                           "--ready-toggle-percent", "0", "--input-interval-ms", "50"]
                bot = subprocess.Popen(command, stdout=bot_log, stderr=subprocess.STDOUT)
                meta["bot_command"] = command
                meta["server_pid"], meta["bot_pid"] = server.pid, bot.pid
                start = time.monotonic()
                meta["sample_start_unix_ms"] = time.time() * 1000 + args.warmup * 1000
                meta["sample_end_unix_ms"] = meta["sample_start_unix_ms"] + args.seconds * 1000
                pids = {"server": server.pid, "bot": bot.pid, **dict(args.observe_pid)}
                previous = {}
                ticks_per_second = os.sysconf("SC_CLK_TCK")
                with (run_dir / "resources.jsonl").open("w") as output:
                    while time.monotonic() - start < args.warmup + args.seconds:
                        if server.poll() is not None or bot.poll() is not None:
                            raise RuntimeError("server/bot exited during measurement; see logs")
                        now = time.monotonic()
                        processes = {}
                        for name, pid in pids.items():
                            item = proc_sample(pid)
                            if item:
                                old = previous.get(name)
                                item["cpu_percent"] = ((item["cpu_ticks"] - old[1]) / ticks_per_second
                                                       / (now - old[0]) * 100) if old else 0
                                previous[name] = (now, item["cpu_ticks"])
                            processes[name] = item
                        output.write(json.dumps({"unix_ms": time.time() * 1000, "processes": processes}) + "\n")
                        output.flush()
                        time.sleep(1)
                bot.terminate()
                bot.wait(timeout=15)
                # Observe connection/room cleanup separately from the load window.
                time.sleep(2)
                meta["bot_exit_code"] = bot.returncode
                meta["database_counts"] = command_output([
                    args.mysql, f"--defaults-extra-file={auth}", "-N", "-B", schema, "-e",
                    "SELECT COUNT(*), COALESCE(SUM(settled),0) FROM battle"])
            finally:
                for process in (bot, server):
                    if process is not None and process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=15)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait()
                meta["finished_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
                (run_dir / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    result = summarize(run_dir)
    if not all(result["audit"].values()):
        raise RuntimeError(f"bot audit failed: {result['audit']}; see {run_dir}")
    print(json.dumps({"run": run_dir.name, "seconds": result["server"]["actual_seconds"],
                      "connections": result["server"]["gauges"]["connections"],
                      "loop_p99_us": result["server"]["histograms"]["loop_work_us"]["p99_us"],
                      "input_wait_p99_us": result["server"]["histograms"]["input_queue_wait_us"]["p99_us"]}),
          flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path)
    parser.add_argument("--bot", type=Path)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--server-revision", help="commit used to build the unchanged server")
    parser.add_argument("--bot-revision", help="frozen commit used to build the bot and sampling script")
    parser.add_argument("--bot-threads", type=int, default=4)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--counts", default="50,100,200")
    parser.add_argument("--warmup", type=int, default=20)
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--mysql", default="mysql")
    parser.add_argument("--observe-pid", action="append", default=[], metavar="NAME=PID")
    parser.add_argument("--summarize-only", action="store_true")
    args = parser.parse_args()
    if args.summarize_only:
        results = [summarize(p) for p in sorted(args.output.iterdir()) if (p / "metadata.json").exists()]
    else:
        if not all((args.server, args.bot, args.config, args.server_revision, args.bot_revision)) or args.seconds < 3 or args.warmup < 0:
            parser.error("server, bot, config, both revisions, seconds >= 3, warmup >= 0 are required")
        if args.bot_threads <= 0 or not 0 <= args.seed < 2**64:
            parser.error("bot-threads must be positive; seed must be uint64")
        args.versions = {
            "server": version_record(args.server_revision, ["src", "proto", "config", "CMakeLists.txt"]),
            "bot": version_record(args.bot_revision, ["bot", "scripts/measure_baseline.py"])}
        args.server, args.bot = args.server.resolve(), args.bot.resolve()
        args.observe_pid = [(v.split("=", 1)[0], int(v.split("=", 1)[1])) for v in args.observe_pid]
        counts = [int(v) for v in args.counts.split(",")]
        if not counts or any(v <= 0 for v in counts):
            parser.error("counts must be positive")
        args.output = args.output.resolve()
        args.output.mkdir(parents=True, exist_ok=False)
        environment = {"utc_time": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                       "platform": platform.platform(), "cpu_count": os.cpu_count(),
                       "cpu_affinity": sorted(os.sched_getaffinity(0)),
                       "cpuinfo": Path("/proc/cpuinfo").read_text().split("\n\n")[0],
                       "meminfo": Path("/proc/meminfo").read_text(),
                       "rlimit_nofile": resource.getrlimit(resource.RLIMIT_NOFILE),
                       "rlimit_nproc": resource.getrlimit(resource.RLIMIT_NPROC),
                       "versions": args.versions, "bot_threads": args.bot_threads, "seed": args.seed,
                       "git_head": command_output(["git", "rev-parse", "HEAD"]),
                       "git_diff_stat": command_output(["git", "diff", "--stat"]),
                       "binary_sha256": {k: hashlib.sha256(p.read_bytes()).hexdigest()
                                         for k, p in (("server", args.server), ("bot", args.bot))}}
        (args.output / "environment.json").write_text(json.dumps(environment, indent=2) + "\n")
        config = read_config(args.config)
        results = [run_one(args, args.output, config, i + 1, count) for i, count in enumerate(counts)]
    (args.output / "summary.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()

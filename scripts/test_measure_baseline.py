import json
from pathlib import Path
import tempfile
import unittest

from measure_baseline import audit_bot, histogram_summary, summarize_metrics


class SummaryTest(unittest.TestCase):
    def test_percentiles_merge_samples_not_window_percentiles(self):
        # One slow sample must not dominate 1000 fast samples from another window.
        result = histogram_summary([
            {"count": 1000, "sum_us": 1000, "max_us": 1, "buckets": [[0, 1000]]},
            {"count": 1, "sum_us": 1000, "max_us": 1000, "buckets": [[73, 1]]}])
        self.assertEqual(result["p99_us"], 1)
        self.assertEqual(result["count"], 1001)
        self.assertEqual(result["mean_us"], 2000 / 1001)

    def test_audit_rejects_unaccounted_input_and_recovery(self):
        import collections
        gauges = collections.defaultdict(int, input_enqueued_total=3, input_written_total=2,
                                          input_cancelled_total=1, reconnect_attempts_total=1,
                                          recovery_timeout_total=1)
        self.assertTrue(all(audit_bot(gauges).values()))
        gauges["input_enqueued_total"] += 1
        gauges["recovery_timeout_total"] = 0
        self.assertFalse(audit_bot(gauges)["input_accounted"])
        self.assertFalse(audit_bot(gauges)["recovery_accounted"])

    def test_empty_and_overflow(self):
        self.assertIsNone(histogram_summary([])["p99_us"])
        result = histogram_summary([{"count": 1, "sum_us": 10**12,
                                     "max_us": 10**12, "buckets": [[255, 1]]}])
        self.assertEqual(result["p99_us"], 10**12)

    def test_window_excludes_warmup_and_first_baseline_histogram(self):
        rows = []
        for i in range(5):
            rows.append({"unix_ms": i * 1000, "elapsed_s": i, "interval_s": 1,
                         "counters": {"messages": i * 10}, "gauges": {"connections": 2},
                         "histograms": {"latency": {"count": 1, "sum_us": 1,
                                                    "max_us": 1, "buckets": [[0, 1]]}}})
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "metrics.jsonl"
            path.write_text("\n".join(map(json.dumps, rows)) + "\n")
            result = summarize_metrics(path, 1000, 4000)
        self.assertEqual(result["actual_seconds"], 3)
        self.assertEqual(result["counters_delta"]["messages"], 30)
        self.assertEqual(result["histograms"]["latency"]["count"], 3)
        self.assertEqual(result["all_run_histograms"]["latency"]["count"], 5)


if __name__ == "__main__":
    unittest.main()

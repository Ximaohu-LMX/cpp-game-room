"""Bot-only wire integration tests; GAME_BOT_BINARY points to a built game_bot.
The stdlib peer implements only the messages exercised here, not game logic.
"""
import asyncio
import collections
import json
import os
from pathlib import Path
import struct
import tempfile
import unittest

from measure_baseline import audit_bot, proc_sample


def varint(value):
    result = bytearray()
    while value > 127:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def integer(field, value):
    return varint(field << 3) + varint(value)


def blob(field, value):
    return varint(field << 3 | 2) + varint(len(value)) + value


def fields(data):
    offset = 0
    def read_varint():
        nonlocal offset
        value = shift = 0
        while True:
            byte = data[offset]
            offset += 1
            value |= (byte & 127) << shift
            if byte < 128:
                return value
            shift += 7
    result = {}
    while offset < len(data):
        tag = read_varint()
        wire = tag & 7
        if wire == 0:
            value = read_varint()
        else:
            size = read_varint() if wire == 2 else {1: 8, 5: 4}[wire]
            value = data[offset:offset + size]
            offset += size
        result[tag >> 3] = value
    return result


def packet(msg, body=b"", seq=0):
    return struct.pack('!III', len(body) + 8, msg, seq) + body


def snapshot(player, frame=6, room=77):
    return packet(4002, integer(1, room) + integer(2, frame) +
                  blob(3, integer(1, player) + integer(4, 63)))


class Peer:
    def __init__(self, scenario):
        self.scenario = scenario
        self.inputs = collections.defaultdict(list)
        self.started = set()
        self.tasks = set()
        self.writers = set()
        self.errors = []

    async def accept(self, reader, writer):
        self.tasks.add(asyncio.current_task())
        self.writers.add(writer)
        player = None
        try:
            while True:
                length, msg, seq = struct.unpack('!III', await reader.readexactly(12))
                body = await reader.readexactly(length - 8)
                if msg == 1001:
                    account = fields(body)[1].decode()
                    player = int(account.rsplit('_', 1)[1]) + 1
                    writer.write(packet(1002, integer(2, player) + blob(4, b'test-token'), seq))
                elif msg == 1003:
                    writer.write(packet(1004, b'', seq))
                elif msg == 2001 and player not in self.started:
                    self.started.add(player)
                    # Sticky packets include the initial snapshot (frame 5).
                    writer.write(packet(2002) + packet(2003, integer(1, 77)) +
                                 packet(3005, integer(1, 77) + integer(2, 2)) + snapshot(player, 5))
                elif msg == 4001:
                    self.inputs[player].append(body)
                    if self.scenario == 'rapid_rounds' and len(self.inputs[player]) <= 3:
                        writer.write(packet(4003, integer(1, 77)) + packet(2003, integer(1, 77)) +
                                     packet(3005, integer(1, 77) + integer(2, 2)) + snapshot(player, 5))
                    if self.scenario == 'bad_packet':
                        writer.write(struct.pack('!III', 1, 0, 0))
                        await writer.drain()
                elif msg == 1005:
                    player = fields(body)[1]
                    mode = (player - 1) % 9
                    ack = packet(1006, integer(2, 77), seq)
                    if mode == 0:  # ack followed by fragmented usable snapshot
                        writer.write(ack + snapshot(player)[:7])
                        await writer.drain()
                        await asyncio.sleep(.01)
                        writer.write(snapshot(player)[7:])
                    elif mode == 1:  # state precedes ack
                        writer.write(snapshot(player) + ack)
                    elif mode == 2:
                        writer.write(packet(1006, b'', seq))
                    elif mode == 3:
                        writer.write(packet(1006, integer(1, 1), seq))
                    elif mode == 4:
                        writer.write(ack)  # ack alone must time out, not count as recovery
                    elif mode == 5:
                        writer.write(ack + packet(4003, integer(1, 77)))
                    elif mode == 6:
                        writer.write(ack + snapshot(player, 4))  # regressed frame
                    elif mode == 7:
                        writer.write(ack + snapshot(player + 1000))  # own player missing
                    else:
                        writer.write(ack + snapshot(player, room=78))
                await writer.drain()
        except (asyncio.IncompleteReadError, ConnectionError):
            pass
        except Exception as error:
            self.errors.append(repr(error))
        finally:
            writer.close()
            self.writers.discard(writer)
            self.tasks.discard(asyncio.current_task())


async def exercise(binary, count, threads=4, seed=123, scenario='normal', duration=2):
    peer = Peer(scenario)
    server = await asyncio.start_server(peer.accept, '127.0.0.1', 0, backlog=1024)
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        args = [binary, '--port', str(server.sockets[0].getsockname()[1]),
                '--count', str(count), '--duration', str(duration), '--io-threads', str(threads),
                '--seed', str(seed), '--metrics-file', str(root / 'metrics.jsonl'),
                '--recovery-file', str(root / 'recovery.jsonl'), '--action-jitter-ms', '0',
                '--cancel-match-percent', '0', '--queue-disconnect-percent', '0',
                '--room-disconnect-percent', '0', '--ready-toggle-percent', '0',
                '--playing-disconnect-percent', '100' if scenario == 'recovery' else '0',
                '--max-reconnects', '1', '--reconnect-delay-ms', '1',
                '--recovery-timeout-ms', '300', '--heartbeat-interval-ms', '100']
        process = await asyncio.create_subprocess_exec(*args, stdout=asyncio.subprocess.PIPE,
                                                       stderr=asyncio.subprocess.STDOUT)
        samples = []
        async def observe():
            while process.returncode is None:
                sample = proc_sample(process.pid)
                if sample:
                    samples.append(sample)
                await asyncio.sleep(.02)
        observer = asyncio.create_task(observe())
        try:
            output, _ = await asyncio.wait_for(process.communicate(), duration + 20)
        finally:
            if process.returncode is None:
                process.kill()
                await process.wait()
            await observer
            server.close()
            await server.wait_closed()
            for writer in list(peer.writers):
                writer.close()
            if peer.tasks:
                await asyncio.gather(*list(peer.tasks))
        if process.returncode:
            raise AssertionError(output.decode())
        if peer.errors:
            raise AssertionError(peer.errors)
        rows = [json.loads(line) for line in (root / 'metrics.jsonl').read_text().splitlines()]
        events = [json.loads(line) for line in (root / 'recovery.jsonl').read_text().splitlines()]
        return rows, events, peer.inputs, samples


@unittest.skipUnless(os.environ.get('GAME_BOT_BINARY'), 'set GAME_BOT_BINARY to run wire integration tests')
class BotRuntimeTest(unittest.TestCase):
    def run_bot(self, count, **kwargs):
        result = asyncio.run(exercise(os.environ['GAME_BOT_BINARY'], count, **kwargs))
        final = result[0][-1]['gauges']
        self.assertTrue(all(audit_bot(final).values()), audit_bot(final))
        self.assertEqual(final['login_ok_total'], count)
        return result

    def test_500_connections_use_fixed_workers_and_write_inputs(self):
        rows, _, inputs, samples = self.run_bot(500, duration=3)
        self.assertEqual(max(row['gauges']['io_workers_live'] for row in rows), 4)
        self.assertEqual(max(s['threads'] for s in samples), 6)  # main + reporter + 4 workers
        self.assertEqual(max(row['gauges']['connections_live'] for row in rows), 500)
        self.assertEqual(len(inputs), 500)
        self.assertTrue(all(len(values) >= 5 for values in inputs.values()))
        self.assertEqual(sum(map(len, inputs.values())), rows[-1]['gauges']['input_written_total'])
        self.assertGreater(sum(r['histograms']['bot_input_timer_late_us']['count'] for r in rows), 0)

    def test_old_round_timers_do_not_multiply_input_rate(self):
        _, _, inputs, _ = self.run_bot(4, scenario='rapid_rounds')
        # Three rapid restarts must still leave only one 50 ms input chain per bot.
        self.assertTrue(all(30 <= len(values) <= 50 for values in inputs.values()))

    def test_seed_is_independent_of_worker_assignment(self):
        def first_inputs(threads, seed):
            inputs = self.run_bot(4, threads=threads, seed=seed)[2]
            return {player: values[:10] for player, values in inputs.items()}
        first = first_inputs(1, 123)
        self.assertEqual(first, first_inputs(4, 123))
        self.assertNotEqual(first, first_inputs(4, 124))

    def test_recovery_requires_ack_and_own_valid_state(self):
        rows, events, _, _ = self.run_bot(18, scenario='recovery')
        self.assertEqual(collections.Counter(e['outcome'] for e in events),
                         {'state': 4, 'no_room': 2, 'rejected': 2, 'timeout': 4, 'terminal': 2, 'invalid': 4})
        final = rows[-1]['gauges']
        self.assertEqual(final['reconnect_attempts_total'], 18)
        self.assertEqual(final['reconnect_ok_total'], 16)
        self.assertEqual(final['recovery_state_total'], 4)
        self.assertEqual(sum(r['histograms']['bot_recovery_state_us']['count'] for r in rows), 4)
        self.assertTrue(all(e['before_frame'] == 5 and e['observed_frame'] == 6
                            for e in events if e['outcome'] == 'state'))

    def test_malformed_peer_closes_once_and_drains_pending_callbacks(self):
        rows, _, _, _ = self.run_bot(32, scenario='bad_packet')
        self.assertEqual(rows[-1]['gauges']['unexpected_disconnects_total'], 32)


if __name__ == '__main__':
    unittest.main()

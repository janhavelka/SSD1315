#!/usr/bin/env python3
"""Regression tests for the SSD1315 HIL runner serial classifier."""

from __future__ import annotations

import contextlib
import io
import tempfile
import unittest
from unittest import mock

import run_ssd1315_hil as hil

HEALTH_VERBOSE = (
    "[I] === Driver Health ===\n[I] State: READY\n[I] Online: true\n"
    "[I] Control state dirty: false\n[I] Consecutive failures: 0\n"
    "[I] Total success: 10\n[I] Total failures: 0\n[I] Success rate: 100.0%\n"
    "[I] === Timestamps ===\n[I] Last success: 2 ms ago (at 10 ms)\n"
    "[I] Last failure: never\n[I] === Last Error ===\n[I] Last error: none\n"
)
TELEMETRY_COMPLETE = (
    "Telemetry:\n uptimeMs=100\n loopHeartbeat=100\n lastLoopMs=95\n"
    " freeHeap=1000\n minFreeHeap=900\n resetReason=1 (poweron)\n"
    " driverState=READY online=true dirty=false controlDirty=false totalSuccess=10 totalFailures=0\n"
)


class HilRunnerParserTest(unittest.TestCase):
    def classify(self, command: str, text: str, visual: bool = False):
        spec = hil.HilCommand(command, visual_check=visual)
        return hil.classify_serial(spec, text)

    def result(self, command: str, serial: str = "PASS", operator: str = "N/A"):
        return hil.CommandResult(
            command=command,
            serial_result=serial,
            operator_result=operator,
            wait_reason="serial-idle",
            elapsed_s=0.1,
            note="",
            raw_excerpt="",
            clean_excerpt="",
        )

    def test_zero_failure_counters_are_not_failures(self) -> None:
        samples = (
            "\x1b[32mfail=0\x1b[0m OK",
            "Selftest result: pass=20 FAIL:0 skip=0",
            "Stress complete\n  Successes: 100\n  Failures: 0\n",
            "Last error: never\nStatus: OK\n",
        )
        for sample in samples:
            with self.subTest(sample=sample):
                self.assertFalse(hil.has_failure_token(hil.strip_ansi(sample)))

    def test_nonzero_failure_counters_are_failures(self) -> None:
        samples = (
            "Selftest result: pass=19 fail=1",
            "Stress complete\n  Failures: 1\n",
            "FAIL:1",
        )
        for sample in samples:
            with self.subTest(sample=sample):
                self.assertTrue(hil.has_failure_token(hil.strip_ansi(sample)))

    def test_transport_error_tokens_are_failures(self) -> None:
        tokens = (
            "I2C_TIMEOUT",
            "DEVICE_NOT_FOUND",
            "STATE_ERROR",
            "BUS_ERROR",
            "I2C_NACK_ADDR",
        )
        for token in tokens:
            with self.subTest(token=token):
                result, reason, _ = self.classify("probe", f"Status: {token}")
                self.assertEqual("FAIL", result)
                self.assertIn("failure token", reason)

    def test_unknown_command_is_terminal_failure(self) -> None:
        text = "[ERROR] unknown command\r\nTM_CLI_RESPONSE_END\r\n"
        for command in ("version", "scan", "clear"):
            with self.subTest(command=command):
                result, reason, _ = self.classify(command, text)
                self.assertEqual("FAIL", result)
                self.assertIn("failure token", reason)
                self.assertTrue(hil.response_has_completion(hil.HilCommand(command), text))

    def test_scan_expected_address_must_appear_in_grid_not_footer(self) -> None:
        text = (
            "30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n"
            "50: 50 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n"
            "Scan complete. Found 1 device(s).\n"
            "Common addresses: 0x3C/0x3D=OLED, 0x51=RV3032\n"
        )
        command = hil.HilCommand("scan")
        result, reason, parsed = hil.classify_serial(
            command, text, hil.Expectations(address=0x3C)
        )
        self.assertEqual("FAIL", result)
        self.assertIn("not found", reason)
        self.assertEqual([0x50], parsed["scan_addresses"])

    def test_scan_exact_grid_address_passes(self) -> None:
        text = (
            "30: -- -- -- -- -- -- -- -- -- -- -- -- 3C -- -- --\n"
            "Scan complete. Found 1 device(s).\n"
        )
        result, _, parsed = hil.classify_serial(
            hil.HilCommand("scan"), text, hil.Expectations(address=0x3C)
        )
        self.assertEqual("PASS", result)
        self.assertEqual([0x3C], parsed["scan_addresses"])

    def test_version_identity_enforces_55_03_311_n16r8_runtime(self) -> None:
        text = (
            "=== Version Info ===\n"
            "Framework: Arduino\n"
            "MCU: ESP32-S3 revision=2\n"
            "Arduino-ESP32: 3.3.11\n"
            "ESP-IDF: v5.5.5\n"
            "Flash bytes: 16777216\n"
            "PSRAM: ready bytes=8388608\n"
            "SSD1315 library version: 4.0.1\n"
            "SSD1315 library commit: abc1234 (clean)\n"
            "Controller profile: SSD1315\n"
            "Panel profile: example-default-128x64-internal-charge-pump\n"
            "Active I2C address: 0x3C\n"
            "Geometry: 128x64 pages=8 pageBufferPages=8\n"
        )
        expected = hil.Expectations(
            address=0x3C,
            width=128,
            height=64,
            controller="SSD1315",
            panel_profile="example-default-128x64-internal-charge-pump",
            commit="abc1234",
            chip_model="ESP32-S3",
            arduino_core="3.3.11",
            esp_idf="5.5.5",
            flash_bytes=16777216,
            psram_bytes=8388608,
        )
        result, _, parsed = hil.classify_serial(hil.HilCommand("version"), text, expected)
        self.assertEqual("PASS", result)
        self.assertEqual("ESP32-S3", parsed["chip_model"])
        self.assertEqual(2, parsed["chip_revision"])
        self.assertEqual("3.3.11", parsed["arduino_core_version"])
        self.assertEqual("v5.5.5", parsed["esp_idf_version"])
        self.assertEqual(16777216, parsed["flash_bytes"])
        self.assertTrue(parsed["psram_ready"])
        self.assertEqual(8388608, parsed["psram_bytes"])

        mismatches = (
            ("Arduino-ESP32: 3.3.11", "Arduino-ESP32: 3.2.0"),
            ("Flash bytes: 16777216", "Flash bytes: 4194304"),
            ("PSRAM: ready bytes=8388608", "PSRAM: not-ready bytes=0"),
        )
        for current, stale in mismatches:
            with self.subTest(stale=stale):
                bad_result, _, _ = hil.classify_serial(
                    hil.HilCommand("version"), text.replace(current, stale), expected
                )
                self.assertEqual("FAIL", bad_result)

        missing_result, _, _ = hil.classify_serial(
            hil.HilCommand("version"),
            text.replace("Arduino-ESP32: 3.3.11\n", ""),
            expected,
        )
        self.assertEqual("FAIL", missing_result)

    def test_serial_open_deasserts_line_state_before_open(self) -> None:
        snapshots = []

        class FakePort:
            def __init__(self, **kwargs) -> None:
                self.port = kwargs["port"]
                self.dtr = True
                self.rts = True

            def open(self) -> None:
                snapshots.append((self.port, self.dtr, self.rts))

            def close(self) -> None:
                pass

        args = hil.parse_args(["--port", "COM21"])
        port = hil.open_serial(type("SerialModule", (), {"Serial": FakePort}), args)
        self.assertEqual([("COM21", False, False)], snapshots)
        self.assertEqual("COM21", port.port)

    def test_stale_prompt_cannot_complete_fragmented_version(self) -> None:
        class FakePort:
            def __init__(self, chunks) -> None:
                self.chunks = list(chunks)

            @property
            def in_waiting(self) -> int:
                return len(self.chunks[0]) if self.chunks else 0

            def read(self, _size: int) -> bytes:
                return self.chunks.pop(0) if self.chunks else b""

        command = hil.HilCommand("version")
        response, wait = hil.read_until_ready(
            FakePort([
                b"\n> ",
                b"SSD1315 library version: 4.0.1\n",
                b"Geometry: 128x64 pages=8 pageBufferPages=8\n",
            ]),
            0.1,
            0.0,
            command,
        )
        self.assertEqual("serial-idle", wait)
        self.assertIn("Geometry: 128x64", response)

        partial, partial_wait = hil.read_until_ready(
            FakePort([b"\n> ", b"SSD1315 library version: 4.0.1\n"]),
            0.01,
            0.0,
            command,
        )
        self.assertEqual("timeout", partial_wait)
        self.assertNotIn("Geometry:", partial)

    def test_visual_command_ok_requires_operator(self) -> None:
        result, reason, _ = self.classify("clear", "Status: OK", visual=True)
        self.assertEqual("SERIAL_PASS_OPERATOR_REQUIRED", result)
        self.assertIn("operator", reason.lower())

    def test_stress_counters_match_requested_count(self) -> None:
        text = "Results:\n  Successes: 100\n  Failures: 0\n"
        result, reason, parsed = self.classify("stress 100", text, visual=True)
        self.assertEqual("SERIAL_PASS_OPERATOR_REQUIRED", result)
        self.assertIn("N=100", reason)
        self.assertEqual(100, parsed["counter_successes"])
        self.assertEqual(0, parsed["counter_failures"])

    def test_stress_mix_counters_match_requested_count(self) -> None:
        text = (
            "=== Stress Mix Test ===\n"
            "Running 500 mixed operations\n"
            "Results:\n"
            "  Total ops: 500\n"
            "  Successes: 500\n"
            "  Failures: 0\n"
            "  setContrast    ok=125 fail=0\n"
        )
        result, reason, parsed = self.classify("stress_mix 500", text, visual=True)
        self.assertEqual("SERIAL_PASS_OPERATOR_REQUIRED", result)
        self.assertIn("N=500", reason)
        self.assertEqual(500, parsed["counter_successes"])
        self.assertEqual(0, parsed["counter_failures"])

    def test_stress_completion_waits_for_results(self) -> None:
        command = hil.HilCommand("stress_mix 500")
        self.assertFalse(hil.response_has_completion(command, "Running 500 mixed operations\n"))
        self.assertTrue(hil.response_has_completion(command, "Results:\nSuccesses: 500\nFailures: 0\n"))

    def test_compact_soakstep_is_complete_and_count_verified(self) -> None:
        command = hil.HilCommand("soakstep 500", visual_check=True)
        text = "SOAK n=500 o=500 f=0 do=500 df=0 s=READY c=0\n"
        self.assertTrue(hil.response_has_completion(command, text))
        self.assertFalse(hil.response_has_completion(
            command,
            "SOAK n=500 o=500 f=0 do=500",
        ))
        result, reason, parsed = hil.classify_serial(command, text)
        self.assertEqual("SERIAL_PASS_OPERATOR_REQUIRED", result)
        self.assertIn("N=500", reason)
        self.assertEqual(500, parsed["counter_operations"])
        self.assertEqual(500, parsed["counter_successes"])
        self.assertEqual(0, parsed["counter_failures"])
        self.assertEqual(500, parsed["driver_success_delta"])
        self.assertEqual(0, parsed["driver_failure_delta"])

        unhealthy = text.replace("df=0", "df=1")
        result, reason, _ = hil.classify_serial(command, unhealthy)
        self.assertEqual("FAIL", result)
        self.assertIn("not clean", reason)

        for mismatched in (
            text.replace("n=500", "n=499"),
            text.replace("o=500", "o=499"),
        ):
            result, reason, _ = hil.classify_serial(command, mismatched)
            self.assertEqual("FAIL", result)
            self.assertIn("do not reconcile", reason)

        maximum_record = (
            "SOAK n=10000 o=0 f=10000 do=0 df=10000 "
            "s=DEGRADED c=65535\n"
        )
        self.assertLessEqual(len(maximum_record.encode("ascii")), 64)

    def test_visual_command_completion_accepts_ok(self) -> None:
        command = hil.HilCommand("scroll stop", visual_check=True)
        self.assertFalse(hil.response_has_completion(command, "[I] > scroll stop\n"))
        self.assertTrue(hil.response_has_completion(command, "[I] scroll stop: OK\n"))

    def test_monitor_accepts_arduino_and_idf_wording(self) -> None:
        for text in ("Health monitor: ON (interval=1000 ms)", "Monitor: ON interval=1000ms"):
            with self.subTest(text=text):
                result, reason, _ = self.classify("monitor 1000", text)
                self.assertEqual("PASS", result)
                self.assertIn("acknowledged", reason)

    def test_telemetry_parses_required_fields(self) -> None:
        text = (
            "Telemetry:\n"
            "  uptimeMs=123456\n"
            "  loopHeartbeat=1000\n"
            "  lastLoopMs=123455\n"
            "  freeHeap=251000\n"
            "  minFreeHeap=240000\n"
            "  resetReason=1 (poweron)\n"
            "  driverState=READY online=true dirty=false controlDirty=false totalSuccess=10 totalFailures=0\n"
        )
        result, reason, parsed = self.classify("telemetry", text)
        self.assertEqual("PASS", result)
        self.assertIn("telemetry parsed", reason)
        self.assertEqual(123456, parsed["uptime_ms"])
        self.assertEqual(1000, parsed["loop_heartbeat"])
        self.assertEqual(251000, parsed["free_heap"])
        self.assertEqual(240000, parsed["min_free_heap"])
        self.assertEqual(1, parsed["reset_reason_code"])
        self.assertEqual("poweron", parsed["reset_reason"])

    def test_telemetry_zero_heap_fails(self) -> None:
        text = (
            "Telemetry:\n"
            "  uptimeMs=1\n"
            "  loopHeartbeat=1\n"
            "  lastLoopMs=1\n"
            "  freeHeap=0\n"
            "  minFreeHeap=0\n"
            "  resetReason=1 (poweron)\n"
        )
        result, reason, _ = self.classify("telemetry", text)
        self.assertEqual("FAIL", result)
        self.assertIn("zero heap", reason)

    def test_benchmark_commands_parse_counts(self) -> None:
        for command in ("flushstress 10", "burst 10"):
            with self.subTest(command=command):
                text = "Results:\n  Successes: 10\n  Failures: 0\n"
                result, reason, parsed = self.classify(command, text)
                self.assertEqual("PASS", result)
                self.assertIn("N=10", reason)
                self.assertEqual(10, parsed["counter_successes"])
                self.assertEqual(0, parsed["counter_failures"])

    def test_parser_self_test_entrypoint_passes(self) -> None:
        self.assertEqual(0, hil.parser_self_test())

    def test_timeout_aliases_and_duration_hours(self) -> None:
        args = hil.parse_args([
            "--dry-run",
            "--timeout-s", "3",
            "--idle-timeout-s", "0.2",
            "--boot-settle-s", "1.5",
            "--soak-duration-hours", "0.001",
            "--soak-read-retries", "3",
            "--soak-read-retry-delay-s", "0.25",
        ])
        self.assertEqual(3.0, args.timeout)
        self.assertEqual(0.2, args.idle_gap)
        self.assertEqual(1.5, args.startup_wait)
        self.assertAlmostEqual(3.6, args.soak_duration_s)
        self.assertEqual(3, args.soak_read_retries)
        self.assertEqual(0.25, args.soak_read_retry_delay_s)

        for invalid in (0, 10001):
            with self.subTest(invalid=invalid):
                with contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit):
                        hil.parse_args(["--dry-run", "--soak-ops", str(invalid)])

        invalid_args = (
            ("--soak-duration-s", "-1"),
            ("--soak-duration-s", "inf"),
            ("--soak-duration-hours", "-1"),
            ("--soak-duration-hours", "nan"),
            ("--soak-max-cycles", "-1"),
            ("--soak-read-retries", "-1"),
            ("--soak-read-retry-delay-s", "-1"),
        )
        for option, value in invalid_args:
            with self.subTest(option=option):
                with contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit):
                        hil.parse_args(["--dry-run", option, value])

    def test_soak_read_retry_is_same_handle_read_only_bounded_and_failure_visible(self) -> None:
        version = hil.HilCommand("version")
        self.assertTrue(hil.should_retry_read_after_interruption(
            version, True, 0, 2, "timeout", "partial version output"
        ))
        self.assertFalse(hil.should_retry_read_after_interruption(
            version, True, 2, 2, "timeout", "partial version output"
        ))
        self.assertFalse(hil.should_retry_read_after_interruption(
            hil.HilCommand("clear"), True, 0, 2, "timeout", ""
        ))
        self.assertFalse(hil.should_retry_read_after_interruption(
            version, False, 0, 2, "timeout", ""
        ))
        self.assertFalse(hil.should_retry_read_after_interruption(
            version, True, 0, 2, "serial-error", ""
        ))
        self.assertFalse(hil.should_retry_read_after_interruption(
            version, True, 0, 2, "timeout", "I2C_TIMEOUT"
        ))

    def test_any_unrecovered_timeout_stops_before_next_transaction(self) -> None:
        args = hil.parse_args(["--dry-run"])
        partial = hil.CommandResult(
            command="soakstep 500",
            serial_result="REVIEW_REQUIRED",
            operator_result="N/A",
            wait_reason="timeout",
            elapsed_s=160.0,
            note="partial response",
            raw_excerpt="[I] > soakstep 500\n",
            clean_excerpt="[I] > soakstep 500\n",
        )
        self.assertTrue(hil.should_stop_after_result(args, partial))

        args.continue_on_fail = True
        self.assertFalse(hil.should_stop_after_result(args, partial))

    def test_run_commands_retries_read_only_on_one_handle_and_runs_cap_cleanup(self) -> None:
        class FakeSerial:
            def __init__(self) -> None:
                self.writes = []
                self.reset_count = 0
                self.close_count = 0

            def write(self, data: bytes) -> int:
                self.writes.append(data)
                return len(data)

            def flush(self) -> None:
                pass

            def reset_input_buffer(self) -> None:
                self.reset_count += 1

            def close(self) -> None:
                self.close_count += 1

        fake = FakeSerial()
        version_ok = (
            "SSD1315 library version: 4.0.1\n"
            "Geometry: 128x64 pages=8 pageBufferPages=8\n"
        )
        telemetry_ok = (
            "Telemetry:\n  uptimeMs=100\n  loopHeartbeat=10\n"
            "  lastLoopMs=99\n"
            "  freeHeap=1000\n  minFreeHeap=900\n"
            "  resetReason=11 (other)\n"
            "  driverState=READY online=true dirty=false controlDirty=false totalSuccess=10 totalFailures=0\n"
        )
        cfg_ok = (
            "Config:\n  initialized=yes dirty=no flushing=no "
            "controlDirty=no scrollActive=no\n"
        )
        reads = (
            ("", "timeout"),
            ("partial version", "timeout"),
            (version_ok, "serial-idle"),
            (telemetry_ok, "serial-idle"),
            (cfg_ok, "serial-idle"),
        )
        commands = (
            hil.HilCommand("version", duration_phase="prologue"),
            hil.HilCommand("telemetry"),
            hil.HilCommand("cfg", duration_phase="epilogue"),
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            args = hil.parse_args([
                "--mode", "soak", "--port", "FAKE", "--out", temp_dir,
                "--startup-wait", "0", "--soak-duration-s", "3600",
                "--soak-max-cycles", "1", "--soak-read-retries", "1",
                "--soak-read-retry-delay-s", "0", "--serial-only",
            ])
            with mock.patch.object(hil, "_load_serial_module", return_value=object()), \
                    mock.patch.object(hil, "open_serial", return_value=fake) as open_mock, \
                    mock.patch.object(hil, "read_until_ready", side_effect=reads):
                _, results, metadata = hil.run_commands(
                    args, commands, hil.Expectations()
                )

        open_mock.assert_called_once()
        self.assertEqual(
            [b"version\n", b"version\n", b"telemetry\n", b"cfg\n"],
            fake.writes,
        )
        self.assertEqual(1, fake.reset_count)
        self.assertEqual(1, fake.close_count)
        self.assertEqual(1, metadata["serial_read_retry_count"])
        self.assertEqual(1, len(metadata["serial_interruptions"]))
        self.assertEqual(
            "soak_max_cycles_before_duration_final_cleanup",
            metadata["stop_reason"],
        )
        self.assertEqual(["version", "telemetry", "cfg"], [r.command for r in results])
        verdicts = hil.verdicts_for("soak", results, metadata)
        self.assertTrue(verdicts["soak_final_cleanup_complete"])
        self.assertFalse(verdicts["soak_duration_met"])
        self.assertFalse(hil.run_exit_pass("soak", verdicts))

    def test_run_commands_does_not_retry_serial_exceptions(self) -> None:
        class FakeSerial:
            def __init__(self) -> None:
                self.writes = []
                self.reset_count = 0
                self.close_count = 0

            def write(self, data: bytes) -> int:
                self.writes.append(data)
                return len(data)

            def flush(self) -> None:
                pass

            def reset_input_buffer(self) -> None:
                self.reset_count += 1

            def close(self) -> None:
                self.close_count += 1

        fake = FakeSerial()
        with tempfile.TemporaryDirectory() as temp_dir:
            args = hil.parse_args([
                "--mode", "functional", "--port", "FAKE", "--out", temp_dir,
                "--startup-wait", "0", "--soak-read-retries", "3",
            ])
            with mock.patch.object(hil, "_load_serial_module", return_value=object()), \
                    mock.patch.object(hil, "open_serial", return_value=fake) as open_mock, \
                    mock.patch.object(
                        hil, "read_until_ready",
                        side_effect=(("", "timeout"), OSError("serial lost")),
                    ):
                _, results, metadata = hil.run_commands(
                    args, (hil.HilCommand("telemetry"),), hil.Expectations()
                )

        open_mock.assert_called_once()
        self.assertEqual([b"telemetry\n"], fake.writes)
        self.assertEqual(0, fake.reset_count)
        self.assertEqual(1, fake.close_count)
        self.assertEqual(0, metadata["serial_read_retry_count"])
        self.assertEqual([], metadata["serial_interruptions"])
        self.assertEqual("FAIL", results[0].serial_result)
        self.assertEqual("serial-error", results[0].wait_reason)

    def test_serial_commands_require_full_writes_without_drain(self) -> None:
        class FakeSerial:
            def __init__(self, short):
                self.short = short
                self.writes = []
                self.flush_calls = 0
                self.close_count = 0

            def write(self, data):
                self.writes.append(data)
                return len(data) - 1 if self.short else len(data)

            def flush(self):
                self.flush_calls += 1
                raise AssertionError("Unbounded serial flush must not run")

            def close(self):
                self.close_count += 1

        telemetry = ("Telemetry:\n  uptimeMs=100\n  loopHeartbeat=10\n"
                     "  lastLoopMs=99\n  freeHeap=1000\n  minFreeHeap=900\n  resetReason=11 (other)\n"
                     "  driverState=READY online=true dirty=false controlDirty=false totalSuccess=10 totalFailures=0\n")
        for short in (False, True):
            with self.subTest(short=short), tempfile.TemporaryDirectory() as temp_dir:
                fake = FakeSerial(short)
                args = hil.parse_args(["--mode", "functional", "--port", "FAKE", "--out", temp_dir,
                                       "--startup-wait", "0", "--soak-read-retries", "3", "--serial-only"])
                commands = (hil.HilCommand("telemetry"),)
                if short:
                    commands += (hil.HilCommand("cfg"),)
                with mock.patch.object(hil, "_load_serial_module", return_value=object()), \
                        mock.patch.object(hil, "open_serial", return_value=fake), \
                        mock.patch.object(hil, "read_until_ready", side_effect=(("", "timeout"), (telemetry, "serial-idle"))) as read:
                    _, results, metadata = hil.run_commands(args, commands, hil.Expectations())
                self.assertEqual(fake.flush_calls, 0)
                self.assertEqual(fake.close_count, 1)
                self.assertEqual(fake.writes, [b"telemetry\n"])
                self.assertEqual(read.call_count, 1 if short else 2)
                self.assertEqual(results[0].serial_result, "FAIL" if short else "PASS")
                self.assertEqual(metadata["serial_read_retry_count"], 0)

    def test_intermediate_cfg_can_allow_dirty_framebuffer(self) -> None:
        text = (
            "Config\n"
            "controllerProfile=SSD1315 panelProfile=WISEVISION_128X64 addr=0x3C geometry=128x64\n"
            "initialized=yes dirty=yes flushing=no controlDirty=no scrollActive=no\n"
        )
        result, reason, _ = hil.classify_serial(hil.HilCommand("cfg", require_clean_cfg=False), text)
        self.assertEqual("PASS", result)
        self.assertIn("not required", reason)

    def test_cfg_last_error_never_does_not_fail(self) -> None:
        text = (
            "Config\n"
            "controllerProfile=SSD1315 panelProfile=WISEVISION_128X64 addr=0x3C geometry=128x64\n"
            "comPins=0x12 chargePump=0x14 iref=0x10 vcomh=0x20\n"
            "initialized=yes dirty=no flushing=no controlDirty=no scrollActive=no\n"
            "Last error: never\n"
        )
        result, reason, parsed = self.classify("cfg", text)
        self.assertEqual("PASS", result)
        self.assertIn("clean-state", reason)
        self.assertEqual("SSD1315", parsed["controller_profile"])
        self.assertEqual(0x3C, parsed["i2c_address"])
        self.assertEqual(0x12, parsed["com_pins"])
        self.assertEqual(0x14, parsed["charge_pump"])
        self.assertEqual(0x10, parsed["iref"])
        self.assertEqual(0x20, parsed["vcomh"])

    def test_cfg_parses_split_width_height_fields(self) -> None:
        parsed = hil.parse_cfg(
            "Config:\n"
            "  width=128 height=64 addr=0x3C\n"
            "  initialized=yes dirty=no flushing=no controlDirty=no scrollActive=no\n"
        )
        self.assertEqual(128, parsed["width"])
        self.assertEqual(64, parsed["height"])
        self.assertEqual(0x3C, parsed["i2c_address"])

    def test_commit_expectation_applies_to_version_not_cfg(self) -> None:
        expectations = hil.Expectations(
            address=0x3C,
            width=128,
            height=64,
            controller="SSD1315",
            panel_profile="WISEVISION_128X64",
            commit="abc1234",
        )
        cfg = (
            "Config:\n"
            "controllerProfile=SSD1315 panelProfile=WISEVISION_128X64 "
            "addr=0x3C geometry=128x64\n"
            "initialized=yes dirty=no flushing=no controlDirty=no scrollActive=no\n"
        )
        result, _, _ = hil.classify_serial(hil.HilCommand("cfg"), cfg, expectations)
        self.assertEqual("PASS", result)

    def test_review_required_blocks_serial_device_pass(self) -> None:
        verdicts = hil.verdicts_for("functional", [
            self.result("version"),
            self.result("probe", serial="REVIEW_REQUIRED"),
        ])
        self.assertFalse(verdicts["serial_device_pass"])
        self.assertTrue(verdicts["serial_review_required"])

    def test_skipped_visual_checks_are_incomplete(self) -> None:
        verdicts = hil.verdicts_for("functional", [
            self.result("clear", serial="SERIAL_PASS_OPERATOR_REQUIRED", operator="SKIP"),
        ])
        self.assertFalse(verdicts["visual_complete"])
        self.assertFalse(verdicts["visual_pass"])

    def test_soak_requires_final_cleanup_cfg(self) -> None:
        incomplete = hil.verdicts_for("soak", [
            self.result("version"),
            self.result("stress_mix 500", serial="SERIAL_PASS_OPERATOR_REQUIRED",
                        operator="SKIPPED_SERIAL_ONLY"),
        ])
        self.assertFalse(incomplete["soak_final_cleanup_complete"])
        self.assertFalse(incomplete["soak_complete"])

        complete = hil.verdicts_for("soak", [
            self.result("version"),
            self.result("clear", serial="SERIAL_PASS_OPERATOR_REQUIRED",
                        operator="SKIPPED_SERIAL_ONLY"),
            self.result("cfg"),
        ])
        self.assertTrue(complete["soak_final_cleanup_complete"])
        self.assertTrue(complete["soak_complete"])

    def test_duration_soak_requires_measured_target_elapsed(self) -> None:
        results = [self.result("cfg")]
        short = hil.verdicts_for(
            "soak", results,
            {"soak_duration_s": 3600.0, "soak_elapsed_s": 3599.9},
        )
        self.assertFalse(short["soak_duration_met"])
        self.assertFalse(short["soak_complete"])

        complete = hil.verdicts_for(
            "soak", results,
            {"soak_duration_s": 3600.0, "soak_elapsed_s": 3600.0},
        )
        self.assertTrue(complete["soak_duration_met"])
        self.assertTrue(complete["soak_complete"])

        self.assertFalse(hil.run_exit_pass("soak", short))
        self.assertTrue(hil.run_exit_pass("soak", complete))
        self.assertTrue(hil.run_exit_pass("functional", short))

    def test_telemetry_health_detects_reboot_or_stalled_counter(self) -> None:
        first = self.result("telemetry")
        first.parsed = {"uptime_ms": 1000, "loop_heartbeat": 50,
                        "reset_reason_code": 1, "free_heap": 100,
                        "min_free_heap": 90}
        second = self.result("telemetry")
        second.parsed = {"uptime_ms": 10, "loop_heartbeat": 1,
                         "reset_reason_code": 3, "free_heap": 100,
                         "min_free_heap": 90}
        health = hil.telemetry_health([first, second])
        self.assertFalse(health["pass"])
        self.assertEqual(3, len(health["problems"]))
        verdicts = hil.verdicts_for("functional", [first, second])
        self.assertFalse(verdicts["serial_device_pass"])

        stalled = self.result("telemetry")
        stalled.parsed = dict(first.parsed)
        stalled.parsed["uptime_ms"] = 1100
        stalled_health = hil.telemetry_health([first, stalled])
        self.assertFalse(stalled_health["pass"])
        self.assertEqual(1, len(stalled_health["problems"]))
        self.assertIn("loop heartbeat did not increase", stalled_health["problems"][0])

    def test_retention_plan_matches_documented_isolation_sequence(self) -> None:
        expected = (
            "version", "cfg", "recover", "scroll stop", "invert 0", "allon 0",
            "clear", "fill", "clear", "pattern checker", "clear", "demo 1",
            "clear", "cfg", "clear", "contrast 1", "clear", "contrast 127",
            "clear", "fill", "clear", "display off", "display on", "recover",
            "clear", "cfg",
        )
        self.assertEqual(expected, tuple(item.command for item in hil.RETENTION_COMMANDS))

    def test_functional_flip_sequence_rewrites_asymmetric_content_before_wake(self) -> None:
        # Also check the shortened plan: omitting high-brightness visuals must
        # not remove the reference image or the post-flip dirty-state checks.
        expected = (
            "contrast 127", "clear", "rect 4 8 20 12", "line 4 8 42 30",
            "flipx 1", "cfg", "flipx 0", "cfg", "flipy 1", "flipy 0",
            "scrollh right 0 7", "scrollv left 0 7 1", "scroll stop",
            "flush", "display off", "display on", "recover",
        )
        for no_risky_visuals in (False, True):
            with self.subTest(no_risky_visuals=no_risky_visuals):
                plan = hil.command_plan("functional", 1, no_risky_visuals)
                commands = tuple(item.command for item in plan)
                start = commands.index("rect 4 8 20 12") - 2
                self.assertEqual(expected, commands[start:start + len(expected)])
                for index in (4, 6, 8, 9, 13, 14, 15):
                    self.assertTrue(plan[start + index].visual_check)
                for index in (5, 7):
                    self.assertTrue(plan[start + index].require_clean_cfg)

    def test_flip_flush_failure_cannot_be_hidden_by_a_setter_success(self) -> None:
        command = next(item for item in hil.FUNCTIONAL_COMMANDS
                       if item.command == "flipx 1")
        for response in (
            "[I] flipx: I2C_NACK_ADDR\n[I] detail=2 msg=address NACK\n",
            "  Status: I2C_TIMEOUT (code=12, detail=263)\n",
            "  Status: OK (code=0, detail=0)\n"
            "  Status: I2C_NACK_DATA (code=11, detail=3)\n",
        ):
            with self.subTest(response=response):
                result, _, _ = hil.classify_serial(command, response)
                self.assertEqual("FAIL", result)

    def test_post_flip_cfg_rejects_pending_rewrite(self) -> None:
        plan = hil.FUNCTIONAL_COMMANDS
        flip_index = next(index for index, item in enumerate(plan)
                          if item.command == "flipx 1")
        command = plan[flip_index + 1]
        for dirty in ("yes", "0xFF"):
            result, reason, _ = hil.classify_serial(
                command,
                f"Config: initialized=yes dirty={dirty} flush=no "
                "controlDirty=no scrollActive=no\n",
            )
            self.assertEqual("FAIL", result)
            self.assertIn("dirty", reason)

    def test_cfg_requires_each_state_field_in_both_modes_and_aliases(self) -> None:
        fields = ("initialized=yes", "dirty=no", "flushing=no",
                  "controlDirty=no", "scrollActive=no")
        for name in ("cfg", "settings"):
            for require_clean in (True, False):
                spec = hil.HilCommand(name, require_clean_cfg=require_clean,
                                      success_pattern=r"Config:")
                complete = "Config: " + " ".join(fields) + "\n"
                self.assertTrue(hil.response_has_completion(spec, complete))
                self.assertEqual("PASS", hil.classify_serial(spec, complete)[0])
                for missing in fields:
                    with self.subTest(name=name, clean=require_clean, missing=missing):
                        text = complete.replace(missing, "")
                        self.assertFalse(hil.response_has_completion(spec, text))
                        result, reason, _ = hil.classify_serial(spec, text)
                        self.assertEqual("FAIL", result)
                        self.assertIn("state fields", reason)
                        # A terminator or an earlier OK cannot waive evidence.
                        self.assertEqual("FAIL", hil.classify_serial(
                            spec, text + "Status: OK\nTM_CLI_RESPONSE_END\n")[0])
                self.assertEqual("FAIL", hil.classify_serial(
                    spec, "Config: initialized=yes controlDirty=no\n")[0])

    def test_cfg_rejects_malformed_repeated_and_prefixed_state_tokens(self) -> None:
        clean = "Config: initialized=yes dirty=no flush=no controlDirty=no scrollActive=no\n"
        fields = ("initialized=yes", "dirty=no", "flush=no", "controlDirty=no", "scrollActive=no")
        for field in fields:
            name = field.split("=")[0]
            for bad in ("", "?", "nope", "falsehood", "yes;", "0x0garbage", "0x100", "0x"):
                with self.subTest(field=field, bad=bad):
                    text = clean.replace(field, f"{name}={bad}")
                    for require_clean in (True, False):
                        spec = hil.HilCommand("cfg", require_clean_cfg=require_clean)
                        self.assertFalse(hil.response_has_completion(spec, text))
                        self.assertEqual("FAIL", hil.classify_serial(spec, text)[0])
            for extra in (field, f"{name}=yes", f"{name}=nope"):
                self.assertEqual("FAIL", self.classify("cfg", clean + extra + "\n")[0])
            self.assertEqual("FAIL", self.classify("cfg", clean.replace(field, "not_" + field))[0])
        self.assertEqual("FAIL", self.classify("cfg", clean + "flushing=no\n")[0])

    def test_cfg_accepts_real_formats_and_completes_known_bad_states(self) -> None:
        for flush_name in ("flush", "flushing"):
            for dirty in ("no", "false", "0x00", "0X00"):
                text = ("Config:\n[I] scrollActive=\x1b[32mno\x1b[0m\n"
                        f"[I] initialized=yes dirty={dirty} {flush_name}=no controlDirty=false\n")
                self.assertEqual("PASS", self.classify("cfg", text)[0])
                for clean, bad in (("initialized=yes", "initialized=no"),
                                   (f"dirty={dirty}", "dirty=0x01"),
                                   (f"{flush_name}=no", f"{flush_name}=true"),
                                   ("controlDirty=false", "controlDirty=yes"),
                                   ("scrollActive=no", "scrollActive=true")):
                    changed = hil.strip_ansi(text).replace(clean, bad)
                    self.assertTrue(hil.response_has_completion(hil.HilCommand("cfg"), changed))
                    self.assertEqual("FAIL", self.classify("cfg", changed)[0])
                    self.assertEqual("PASS", hil.classify_serial(
                        hil.HilCommand("cfg", require_clean_cfg=False), changed)[0])

    def test_cfg_completion_waits_for_delimited_late_fields(self) -> None:
        class FragmentedPort:
            def __init__(self, chunks):
                self.chunks = list(chunks)

            @property
            def in_waiting(self):
                return len(self.chunks[0]) if self.chunks else 0

            def read(self, _size):
                return self.chunks.pop(0) if self.chunks else b""

        spec = hil.HilCommand("cfg")
        prefix = b"Config: initialized=yes flush=no controlDirty=no scrollActive=no dirty=0x0"
        self.assertFalse(hil.response_has_completion(spec, prefix.decode()))
        response, wait = hil.read_until_ready(
            FragmentedPort([b"\n> ", prefix, b"1\n> "]), 0.1, 0.0, spec)
        self.assertNotEqual("timeout", wait)
        self.assertIn("dirty=0x01", response)
        self.assertEqual("FAIL", hil.classify_serial(spec, response)[0])

    def test_terminal_status_errors_complete_and_override_earlier_success(self) -> None:
        errors = ("TIMEOUT", "CONTROL_STATE_UNKNOWN", "CANCELLED", "RESULT_NOT_AVAILABLE",
                  "BUSY", "PANEL_NOT_READY", "NOT_INITIALIZED", "BUFFER_TOO_SMALL",
                  "BUFFER_OVERFLOW", "UNSUPPORTED", "INTERNAL_ERROR", "DRIVER_OFFLINE")
        for error in errors:
            for status in (f"[I] flipx: \x1b[31m{error}\x1b[0m\n",
                           f"  Status: {error} (code=16, detail=-42)\n"):
                spec = hil.HilCommand("flipx 1", visual_check=True)
                text = "Status: OK\n" + status
                self.assertTrue(hil.response_has_completion(spec, status))
                self.assertEqual("FAIL", hil.classify_serial(spec, text)[0])
        self.assertEqual("FAIL", hil.classify_serial(
            hil.HilCommand("scroll stop"), "[I] scroll stop: TIMEOUT\n")[0])

    def test_status_detection_ignores_partial_progress_and_timeout_history(self) -> None:
        spec = hil.HilCommand("flipx 1", visual_check=True)
        for text in ("[I] flipx: TIME", "Status: OK", "Status: IN_PROGRESS\n"):
            self.assertFalse(hil.response_has_completion(spec, text))
        self.assertNotEqual("FAIL", hil.classify_serial(spec, "Status: IN_PROGRESS\n")[0])
        for text in ("Last error: TIMEOUT\n", "timeout=1000\n", "TIMEOUT counter=0\n"):
            response = text + "[I] flipx: OK\n"
            self.assertEqual("SERIAL_PASS_OPERATOR_REQUIRED", hil.classify_serial(spec, response)[0])

    def test_arduino_status_labels_match_dispatch_and_propagate_errors(self) -> None:
        cases = (("contrast 127", "setContrast"), ("bright 127", "setBrightness"),
                 ("scrollstop", "scroll stop"), ("text hello", "text+flush"),
                 ("clear", "clear+flush"), ("reset", "begin"),
                 ("probe", "Probe result"), ("recover", "Recover result"))
        for name, label in cases:
            spec = hil.HilCommand(name)
            for code in ("OK", "TIMEOUT", "FAILED"):
                text = f"[I] {label}: \x1b[31m{code}\x1b[0m (code=16)\n"
                if name in ("probe", "recover") and code == "OK":
                    health = "Health: state=READY online=true controlDirty=false consecFail=0 ok=10 fail=0\n"
                    text = (f"=== {name.capitalize()} Operation ===\nBefore {name}:\n" + health
                            + text + f"After {name}:\n" + health + "Changes:\n(no changes)\n")
                self.assertTrue(hil.response_has_completion(spec, text))
                expected = "PASS" if code == "OK" else "FAIL"
                self.assertEqual(expected, hil.classify_serial(spec, text)[0])
                if code != "OK":
                    self.assertEqual("FAIL", hil.classify_serial(spec, "Status: OK\n" + text)[0])
            self.assertFalse(hil.response_has_completion(spec, f"[I] {label}: OK"))
            self.assertFalse(hil.response_has_completion(spec, f"[I] {label}: "))

    def test_idf_probe_and_recover_status_ok_complete_but_multistage_summaries_wait(self) -> None:
        for name in ("probe", "recover"):
            spec = hil.HilCommand(name)
            text = "Status: OK (code=0, detail=0)\n"
            self.assertTrue(hil.response_has_completion(spec, text))
            self.assertEqual("PASS", hil.classify_serial(spec, text)[0])
        for name in ("selftest", "demo", "stress 100"):
            self.assertFalse(hil.response_has_completion(hil.HilCommand(name), "Status: OK\n"))

    def test_workload_counts_must_be_complete_valid_and_match_requested_work(self) -> None:
        for name in ("stress 100", "stress_mix 100", "burst 100", "flushstress 100"):
            spec = hil.HilCommand(name)
            for count in (0, 5, 99, 101):
                text = f"Results:\nSuccesses: {count}\nFailures: 0\n"
                self.assertTrue(hil.response_has_completion(spec, text))
                self.assertEqual("FAIL", hil.classify_serial(spec, text)[0])
            for tail in ("", "Failures: 0garbage\n", "Failures:\n", "Failures: 0\nFailures: 0\n"):
                text = "Results:\nSuccesses: 100\n" + tail
                self.assertFalse(hil.response_has_completion(spec, text))
                self.assertNotEqual("PASS", hil.classify_serial(spec, text)[0])
            for text in ("Results:\nTotal ops: 100\nFailures: 0\n",
                         "Results:\nSuccesses: 100\nsetContrast ok=100 fail=0\n"):
                self.assertNotEqual("PASS", hil.classify_serial(
                    spec, text + "TM_CLI_RESPONSE_END\n")[0])
        text = "Selftest result: pass=10 fail=0garbage\n"
        self.assertFalse(hil.response_has_completion(hil.HilCommand("selftest"), text))
        self.assertNotEqual("PASS", self.classify("selftest", text)[0])

    def test_workload_completion_waits_for_delimited_counter_values(self) -> None:
        class FragmentedPort:
            def __init__(self, chunks):
                self.chunks = list(chunks)

            @property
            def in_waiting(self):
                return len(self.chunks[0]) if self.chunks else 0

            def read(self, _size):
                return self.chunks.pop(0) if self.chunks else b""

        cases = (("stress 100", "Results:\nSuccesses: 100\nFailures: 0"),
                 ("selftest", "Selftest result: pass=10 fail=0"),
                 ("soakstep 100", "SOAK n=100 o=100 f=0 do=100 df=0 s=READY c=0"))
        for name, prefix in cases:
            spec = hil.HilCommand(name)
            self.assertFalse(hil.response_has_completion(spec, prefix))
            self.assertTrue(hil.response_has_completion(spec, prefix + "\n"))
            response, wait = hil.read_until_ready(FragmentedPort(
                [b"\n> ", prefix.encode(), b"garbage\nTM_CLI_RESPONSE_END\n> "]), 0.1, 0.0, spec)
            self.assertNotEqual("timeout", wait)
            self.assertIn("0garbage", response)
            self.assertNotEqual("PASS", hil.classify_serial(spec, response)[0])

    def test_runner_stops_on_incomplete_cfg_terminal_timeout_and_short_workload(self) -> None:
        class FakeSerial:
            def __init__(self, payload):
                self.payload = payload
                self.writes = []
                self.pending = b""

            @property
            def in_waiting(self):
                return len(self.pending)

            def read(self, _size):
                data, self.pending = self.pending, b""
                return data

            def write(self, data):
                self.writes.append(data)
                self.pending = self.payload

            def flush(self):
                pass

            def close(self):
                pass

        real_read = hil.read_until_ready
        def read(port, timeout, idle, command=None, **kwargs):
            return ("", "serial-idle") if command is None else real_read(port, timeout, idle, command, **kwargs)

        cases = (("cfg", b"Config: initialized=yes controlDirty=no\nTM_CLI_RESPONSE_END\n> "),
                 ("flipx 1", b"[I] flipx: TIMEOUT\n> "),
                 ("stress 100", b"Results:\nSuccesses: 5\nFailures: 0\n> "))
        for name, response in cases:
            with self.subTest(command=name), tempfile.TemporaryDirectory() as temp_dir:
                args = hil.parse_args(["--mode", "functional", "--port", "FAKE",
                                       "--out", temp_dir, "--startup-wait", "0", "--serial-only"])
                port = FakeSerial(response)
                with mock.patch.object(hil, "_load_serial_module", return_value=object()), \
                        mock.patch.object(hil, "open_serial", return_value=port), \
                        mock.patch.object(hil, "read_until_ready", side_effect=read):
                    _, results, metadata = hil.run_commands(
                        args, (hil.HilCommand(name), hil.HilCommand("clear")), hil.Expectations())
                self.assertEqual([(name + "\n").encode()], port.writes)
                self.assertEqual("FAIL", results[0].serial_result)
                verdicts = hil.verdicts_for("functional", results, metadata)
                self.assertFalse(hil.run_exit_pass("functional", verdicts))

    def test_command_specific_marker_drives_completion_and_pass(self) -> None:
        command = hil.HilCommand(
            "verbose 1",
            success_pattern=r"Verbose mode:.*ON",
        )
        response = "[I] Verbose mode: ON\n"
        result, reason, _ = hil.classify_serial(command, response)
        self.assertEqual("PASS", result)
        self.assertIn("command-specific", reason)
        self.assertTrue(hil.response_has_completion(command, response))

    def test_health_formats_require_every_record_and_reject_duplicate_or_partial_fields(self):
        raw = (
            "Raw Health Counters:\nstate() = 1 (READY)\nisOnline() = true\n"
            "consecutiveFailures()= 0\ntotalSuccess() = 10\ntotalFailures() = 0\n"
            "lastOkMs() = 10\nlastErrorMs() = 0\nlastError().code = 0 (OK)\n"
            'lastError().detail = 0\nlastError().msg = ""\n'
        )
        formats = (
            ("drv", HEALTH_VERBOSE), ("health", HEALTH_VERBOSE), ("counters", raw),
            ("brief", "Health: state=READY online=true controlDirty=false consecFail=0 ok=10 fail=0\n"),
            ("read", "Display: state=READY online=yes init=yes sleep=no flush=no dirty=0x00 controlDirty=no ok=10 fail=0 consec=0\n"),
        )
        for name, complete in formats:
            spec = next((c for c in hil.ARDUINO_EXTENDED_COMMANDS if c.command == name), hil.HilCommand(name))
            with self.subTest(name=name):
                self.assertEqual(hil.classify_serial(spec, complete)[0], "PASS")
                self.assertTrue(hil.response_has_completion(spec, complete))
                for line in complete.splitlines(keepends=True):
                    broken = complete.replace(line, "")
                    self.assertNotEqual(hil.classify_serial(spec, broken)[0], "PASS", line)
                    self.assertFalse(hil.response_has_completion(spec, broken), line)
                self.assertNotEqual(hil.classify_serial(spec, complete + complete)[0], "PASS")
        for broken in (HEALTH_VERBOSE.replace("State: READY", "State: READY\nState: REA"),
                       "=== Driver Health ===\n> ", "Health: state=\n> "):
            self.assertNotEqual(hil.classify_serial(hil.HilCommand("drv"), broken)[0], "PASS")

    def test_health_snapshots_preserve_i2c_error_names(self):
        # Actual HealthDiag.h / printRawCounters formats include the digit in I2C.
        for code in ("I2C_NACK_ADDR", "I2C_NACK_DATA", "I2C_ERROR"):
            history = HEALTH_VERBOSE.replace("[I] Last error: none\n",
                f"[I] Code: {code}\n[I] Detail: 2\n[I] Message: rejected\n")
            self.assertEqual(hil.parse_health(history).get("last_code"), code)
            dirty = history.replace("Control state dirty: false",
                f"Control state dirty: true\n[I] Control state error: {code} detail=2 msg=rejected")
            self.assertEqual(hil.parse_health(dirty).get("last_code"), code)
            self.assertEqual(self.classify("drv", dirty)[0], "FAIL")
            raw = ("Raw Health Counters:\nstate() = 2 (DEGRADED)\nisOnline() = true\n"
                   "consecutiveFailures()= 1\ntotalSuccess() = 10\ntotalFailures() = 1\n"
                   f"lastOkMs() = 10\nlastErrorMs() = 11\nlastError().code = 2 ({code})\n"
                   'lastError().detail = 2\nlastError().msg = "rejected"\n')
            self.assertEqual(hil.parse_health(raw).get("last_code"), code)
            self.assertEqual(self.classify("counters", raw)[0], "FAIL")

    def test_summary_records_cannot_reuse_stale_or_spliced_success(self):
        good = "SOAK n=100 o=100 f=0 do=100 df=0 s=READY c=0\n"
        for tail in (good, "SOAK n=100 o=99 f=1 do=99 df=1 s=DEGRADED c=1\n", "SOAK n=100\n"):
            self.assertNotEqual(self.classify("soakstep 100", good + tail)[0], "PASS")
        for command in ("stress 100", "stress_mix 100", "flushstress 100", "burst 100"):
            self.assertNotEqual(self.classify(command, "Results:\nSuccesses: 100\nResults:\nFailures: 0\n")[0], "PASS")
        self.assertNotEqual(self.classify("selftest", "Selftest result: pass=10\nSelftest result: fail=0\n")[0], "PASS")
        self.assertNotEqual(self.classify("demo", "Demo summary: pass=10\nTM_CLI_RESPONSE_END\n")[0], "PASS")

    def test_version_alias_requires_complete_current_geometry(self):
        for name in ("version", "ver"):
            spec = next((c for c in hil.ARDUINO_EXTENDED_COMMANDS if c.command == name), hil.HilCommand(name))
            for text in ("SSD1315 library version: 4.0.1\n", "SSD1315 library version: 4.0.1\nGeometry: 128x64 pages=8 pageBufferPages=8garbage\n"):
                self.assertFalse(hil.response_has_completion(spec, text))
                self.assertNotEqual(hil.classify_serial(spec, text)[0], "PASS")

    def test_telemetry_checks_full_current_snapshot_and_numeric_boundaries(self):
        self.assertEqual(self.classify("telemetry", TELEMETRY_COMPLETE)[0], "PASS")
        for line in TELEMETRY_COMPLETE.splitlines(keepends=True):
            with self.subTest(line=line):
                self.assertNotEqual(self.classify("telemetry", TELEMETRY_COMPLETE.replace(line, ""))[0], "PASS")
                self.assertFalse(hil.response_has_completion(hil.HilCommand("telemetry"), TELEMETRY_COMPLETE.replace(line, "")))
        for field in ("uptimeMs=100", "loopHeartbeat=100", "lastLoopMs=95", "freeHeap=1000", "minFreeHeap=900", "resetReason=1"):
            self.assertNotEqual(self.classify("telemetry", TELEMETRY_COMPLETE.replace(field, field + "garbage"))[0], "PASS")
        self.assertNotEqual(self.classify("telemetry", TELEMETRY_COMPLETE + " freeHeap=0\n")[0], "PASS")

    def test_crash_reset_and_truncated_status_override_prior_success(self):
        for tail in ("Guru Meditation Error: Core 1 panic\n", "ESP-ROM:esp32s3\n", "assert failed: test\n", "Probe result:\n"):
            with self.subTest(tail=tail):
                self.assertNotEqual(self.classify("probe", "Probe result: OK\n" + tail)[0], "PASS")

    def test_arduino_probe_recover_wait_for_both_health_snapshots_and_change_tail(self):
        health = "Health: state=READY online=true controlDirty=false consecFail=0 ok=10 fail=0\n"
        for name in ("probe", "recover"):
            title = name.capitalize()
            complete = (f"=== {title} Operation ===\nBefore {name}:\n" + health
                        + f"{title} result: OK (code=0: OK)\nAfter {name}:\n" + health
                        + "Changes:\n(no changes)\n")
            spec = hil.HilCommand(name)
            self.assertEqual(hil.classify_serial(spec, complete)[0], "PASS")
            self.assertTrue(hil.response_has_completion(spec, complete))
            for broken in (complete.replace(f"=== {title} Operation ===\n", ""),
                           f"{title} result: OK (code=0: OK)\n",
                           complete.replace(health, "", 1), complete.replace("Changes:\n", ""),
                           complete.replace("(no changes)\n", ""), complete + "unexplained tail\n"):
                with self.subTest(name=name, broken=broken):
                    self.assertFalse(hil.response_has_completion(spec, broken))
                    self.assertNotEqual(hil.classify_serial(spec, broken)[0], "PASS")

    def test_capture_keeps_quiet_tail_and_live_partial_data_on_error(self):
        class Port:
            def __init__(self, chunks): self.chunks = list(chunks)
            @property
            def in_waiting(self): return 4096
            def read(self, size):
                item = self.chunks.pop(0) if self.chunks else b""
                if isinstance(item, Exception): raise item
                return item
        live = []
        response, wait = hil.read_until_ready(Port([b"Probe result: OK\n> ", b"Guru Meditation Error\n"]),
                                              .05, 0, hil.HilCommand("probe"), write_log=live.append)
        self.assertIn("Guru Meditation", response)
        self.assertEqual("".join(live), response)
        self.assertEqual(self.classify("probe", response)[0], "FAIL")
        response, wait = hil.read_until_ready(Port([b"partial sensor response", OSError("USB lost")]),
                                              .05, 0, hil.HilCommand("drv"), write_log=live.append)
        self.assertEqual(response, "partial sensor response")
        self.assertIn("serial-error", wait)
        self.assertIn("partial sensor response", live)

    def test_capture_is_bounded_by_chunk_total_and_deadline(self):
        class Flood:
            in_waiting = 10**9
            def read(self, size):
                self.largest = max(getattr(self, "largest", 0), size)
                return b"x" * size
        flood = Flood()
        with mock.patch.object(hil, "MAX_RESPONSE_BYTES", 8192):
            response, wait = hil.read_until_ready(flood, .1, 0, hil.HilCommand("drv"))
        self.assertEqual(wait, "capture-limit")
        self.assertEqual(flood.largest, 4096)
        self.assertLessEqual(len(response), 8192 + 4096)

    def test_original_startup_gate_handles_arduino_and_idf_without_fake_prompt(self):
        arduino = ("ESP-ROM:esp32s3\n[I] === SSD1315 01_basic_bringup_cli ===\n"
                   "[I] Dimensions: 128x64\n[I] I2C Address: 0x3C\n"
                   "[I] begin() returned: OK (code=0)\n" + HEALTH_VERBOSE + "[I] Ready for commands!\n")
        idf = ("=== SSD1315 native ESP-IDF bringup ===\n" + "Status: OK (code=0, detail=0)\n" * 7
               + "Type 'help' for commands.\n> ")
        for complete in (arduino, idf):
            self.assertIsNone(hil.startup_error(complete, hil.Expectations()))
            for broken in (complete + "ESP-ROM:esp32s3\n", complete + "partial tail", complete + "Guru Meditation Error\n"):
                self.assertIsNotNone(hil.startup_error(broken, hil.Expectations()))
        for line in HEALTH_VERBOSE.splitlines(keepends=True):
            self.assertIsNotNone(hil.startup_error(arduino.replace(line, ""), hil.Expectations()))
        self.assertIsNotNone(hil.startup_error(idf.replace("Status: OK (code=0, detail=0)\n", "", 1), hil.Expectations()))

    def test_required_startup_failure_closes_port_without_commands(self):
        port = mock.Mock()
        with tempfile.TemporaryDirectory() as temp:
            args = hil.parse_args(["--port", "FAKE", "--out", temp, "--require-startup", "--startup-wait", "0"])
            with mock.patch.object(hil, "_load_serial_module", return_value=object()), \
                    mock.patch.object(hil, "open_serial", return_value=port), \
                    mock.patch.object(hil, "read_until_ready", return_value=("partial boot", "timeout")):
                _, results, metadata = hil.run_commands(args, (hil.HilCommand("probe"),), hil.Expectations())
        port.write.assert_not_called()
        port.close.assert_called_once()
        self.assertEqual(results[0].serial_result, "FAIL")
        self.assertEqual(metadata["stop_reason"], "startup_failure")

    def test_arduino_extended_plan_restores_safe_final_state(self) -> None:
        plan = hil.command_plan("arduino-extended", 1)
        commands = [item.command for item in plan]
        for required in (
            "verbose 1", "read", "health", "threshold", "dirty",
            "dirty mark 0 0 127", "contrast", "bright", "display", "sleep", "allon",
            "zoom", "fade", "invert", "flipx", "flipy", "scrollstop",
            "flushrect 8 8 32 16", "fillrect 8 8 32 16", "featuretest",
        ):
            self.assertIn(required, commands)
        for deprecated_noop in ("touch", "userpages", "activepage",
                                "pagecycle", "autosleep"):
            self.assertFalse(any(command == deprecated_noop or
                                 command.startswith(deprecated_noop + " ")
                                 for command in commands))
        self.assertFalse(any(command.startswith("cmd") for command in commands))
        self.assertEqual(("contrast 127", "clear", "cfg"),
                         tuple(item.command for item in plan[-3:]))

    def test_soak_plan_records_telemetry_and_ends_with_cleanup_cfg(self) -> None:
        plan = hil.command_plan("soak", 5)
        self.assertEqual("cfg", plan[-1].command)
        self.assertGreaterEqual(sum(1 for command in plan if command.command == "telemetry"), 3)
        self.assertEqual("soakstep 5", [command.command for command in plan if command.command.startswith("soakstep")][0])

        first = tuple(item.command for item in hil.duration_soak_batch(plan, 1, False))
        repeated = tuple(item.command for item in hil.duration_soak_batch(plan, 2, False))
        cleanup = tuple(item.command for item in hil.duration_soak_batch(plan, 2, True))
        self.assertEqual(("version", "telemetry", "cfg"), first[:3])
        self.assertNotIn("version", repeated)
        self.assertNotIn("cfg", repeated)
        self.assertEqual(("cfg",), cleanup)
        self.assertEqual("telemetry", repeated[-1])

        self.assertEqual(
            "",
            hil.duration_soak_cleanup_reason(0, True, 1),
        )
        self.assertEqual(
            "duration_deadline_after_cycle_cleanup",
            hil.duration_soak_cleanup_reason(1, True, 1),
        )
        self.assertEqual(
            "soak_max_cycles_before_duration_final_cleanup",
            hil.duration_soak_cleanup_reason(2, False, 2),
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)

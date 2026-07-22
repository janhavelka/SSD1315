#!/usr/bin/env python3
"""Regression tests for the SSD1315 HIL runner serial classifier."""

from __future__ import annotations

import unittest

import run_ssd1315_hil as hil


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

    def test_visual_command_ok_requires_operator(self) -> None:
        result, reason, _ = self.classify("clear", "Status: OK", visual=True)
        self.assertEqual("SERIAL_PASS_OPERATOR_REQUIRED", result)
        self.assertIn("operator", reason.lower())

    def test_stress_counters_match_requested_count(self) -> None:
        text = "Stress complete\n  Successes: 100\n  Failures: 0\n"
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
        text = (
            "Results: SoakStep Total ops: 500 Successes: 500 Failures: 0 "
            "elapsedMs=16025 driverOkDelta=625 driverFailDelta=0 "
            "state=READY consecutiveFailures=0\n"
        )
        self.assertTrue(hil.response_has_completion(command, text))
        result, reason, parsed = hil.classify_serial(command, text)
        self.assertEqual("SERIAL_PASS_OPERATOR_REQUIRED", result)
        self.assertIn("N=500", reason)
        self.assertEqual(500, parsed["counter_operations"])
        self.assertEqual(500, parsed["counter_successes"])
        self.assertEqual(0, parsed["counter_failures"])

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

    def test_arduino_extended_plan_restores_safe_final_state(self) -> None:
        plan = hil.command_plan("arduino-extended", 1)
        commands = [item.command for item in plan]
        for required in (
            "verbose 1", "read", "health", "threshold", "dirty",
            "dirty mark 0 0 127", "userpages", "activepage", "pagecycle",
            "autosleep", "contrast", "bright", "display", "sleep", "allon",
            "zoom", "fade", "invert", "flipx", "flipy", "scrollstop",
            "flushrect 8 8 32 16", "fillrect 8 8 32 16", "featuretest",
        ):
            self.assertIn(required, commands)
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


if __name__ == "__main__":
    unittest.main(verbosity=2)

#!/usr/bin/env python3
"""Regression tests for the SSD1315 HIL runner serial classifier."""

from __future__ import annotations

import unittest

import run_ssd1315_hil as hil


class HilRunnerParserTest(unittest.TestCase):
    def classify(self, command: str, text: str, visual: bool = False):
        spec = hil.HilCommand(command, visual_check=visual)
        return hil.classify_serial(spec, text)

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

    def test_visual_command_completion_accepts_ok(self) -> None:
        command = hil.HilCommand("scroll stop", visual_check=True)
        self.assertFalse(hil.response_has_completion(command, "[I] > scroll stop\n"))
        self.assertTrue(hil.response_has_completion(command, "[I] scroll stop: OK\n"))

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


if __name__ == "__main__":
    unittest.main(verbosity=2)

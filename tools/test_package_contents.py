#!/usr/bin/env python3
"""Regression tests for the release-evidence gate; fixtures stay in memory."""

from __future__ import annotations

import contextlib
import io
import unittest

import check_package_contents as package


class ReleaseEvidenceTest(unittest.TestCase):
    REPORT = "docs/reports/hil-validation-COM21-20260731.md"
    TOKENS = ("55.03.311", "97,000 operations", "3,612.265")

    @classmethod
    def setUpClass(cls) -> None:
        cls.report_text = (package.ROOT / cls.REPORT).read_text(encoding="utf-8")

    def assert_rejected(self, text: str, prefix: str, reason: str) -> None:
        output = io.StringIO()
        with contextlib.redirect_stdout(output), self.assertRaises(SystemExit) as caught:
            package.validate_release_evidence({self.REPORT: text}, prefix)
        self.assertEqual(caught.exception.code, 1)
        self.assertIn(prefix, output.getvalue())
        self.assertIn(reason, output.getvalue())

    def test_recorded_evidence_is_accepted(self) -> None:
        for prefix in ("source ", "archive "):
            with self.subTest(prefix=prefix):
                package.validate_release_evidence({self.REPORT: self.report_text}, prefix)

    def test_release_blockers_override_complete_evidence(self) -> None:
        for prefix in ("source ", "archive "):
            for blocker in ("IN_PROGRESS_DO_NOT_RELEASE", "SOAK_PENDING_REPLACE"):
                with self.subTest(prefix=prefix, blocker=blocker):
                    self.assert_rejected(
                        self.report_text + "\n" + blocker, prefix, "release blocker"
                    )

    def test_version_alone_is_insufficient(self) -> None:
        for prefix in ("source ", "archive "):
            with self.subTest(prefix=prefix):
                self.assert_rejected("55.03.311", prefix, "missing evidence token")

    def test_each_evidence_token_is_required(self) -> None:
        for prefix in ("source ", "archive "):
            for token in self.TOKENS:
                with self.subTest(prefix=prefix, token=token):
                    self.assert_rejected(
                        self.report_text.replace(token, ""), prefix,
                        f"missing evidence token '{token}'",
                    )


if __name__ == "__main__":
    unittest.main()

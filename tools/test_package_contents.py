#!/usr/bin/env python3
"""Regression tests for the release-evidence gate; fixtures stay in memory."""

from __future__ import annotations

import contextlib
import io
import unittest

import check_package_contents as package


class ReleaseEvidenceTest(unittest.TestCase):
    EVIDENCE = package.RELEASE_EVIDENCE_PATH
    TOKENS = ("55.03.311", "97,000 operations", "3,612.265")

    @classmethod
    def setUpClass(cls) -> None:
        cls.evidence_text = (package.ROOT / cls.EVIDENCE).read_text(encoding="utf-8")

    def assert_rejected(self, text: str, prefix: str, reason: str) -> None:
        output = io.StringIO()
        with contextlib.redirect_stdout(output), self.assertRaises(SystemExit) as caught:
            package.validate_release_evidence({self.EVIDENCE: text}, prefix)
        self.assertEqual(caught.exception.code, 1)
        self.assertIn(prefix, output.getvalue())
        self.assertIn(reason, output.getvalue())

    def test_recorded_evidence_is_accepted(self) -> None:
        for prefix in ("source ", "archive "):
            with self.subTest(prefix=prefix):
                package.validate_release_evidence({self.EVIDENCE: self.evidence_text}, prefix)

    def test_release_blockers_override_complete_evidence(self) -> None:
        for prefix in ("source ", "archive "):
            for blocker in ("IN_PROGRESS_DO_NOT_RELEASE", "SOAK_PENDING_REPLACE"):
                with self.subTest(prefix=prefix, blocker=blocker):
                    self.assert_rejected(
                        self.evidence_text + "\n" + blocker, prefix, "release blocker"
                    )

    def test_version_alone_is_insufficient(self) -> None:
        for prefix in ("source ", "archive "):
            with self.subTest(prefix=prefix):
                self.assert_rejected(package.RELEASE_EVIDENCE_HEADING + "\n55.03.311",
                                     prefix, "missing evidence token")

    def test_each_evidence_token_is_required(self) -> None:
        for prefix in ("source ", "archive "):
            for token in self.TOKENS:
                with self.subTest(prefix=prefix, token=token):
                    self.assert_rejected(
                        self.evidence_text.replace(token, ""), prefix,
                        f"missing evidence token '{token}'",
                    )


    def test_missing_or_repeated_dated_section_is_rejected(self) -> None:
        for prefix in ("source ", "archive "):
            for text in (self.evidence_text.replace(package.RELEASE_EVIDENCE_HEADING, "### Other run"),
                         self.evidence_text + "\n" + self.evidence_text):
                with self.subTest(prefix=prefix):
                    self.assert_rejected(text, prefix, "one dated COM21 evidence section")

    def test_other_runs_cannot_supply_missing_evidence(self) -> None:
        for prefix in ("source ", "archive "):
            for token in self.TOKENS:
                for heading in ("# Other section", "## Other section", "### Other run",
                                "###\tOther run", "   ## Other section"):
                    remaining = "\n".join(value for value in self.TOKENS if value != token)
                    text = (package.RELEASE_EVIDENCE_HEADING + "\n" + remaining +
                            "\n" + heading + "\n" + token)
                    with self.subTest(prefix=prefix, token=token, heading=heading):
                        self.assert_rejected(text, prefix, f"missing evidence token '{token}'")

    def test_lower_level_subsections_belong_to_dated_evidence(self) -> None:
        text = package.RELEASE_EVIDENCE_HEADING + "\n55.03.311\n#### Soak\n97,000 operations\n3,612.265\n"
        for prefix in ("source ", "archive "):
            package.validate_release_evidence({self.EVIDENCE: text}, prefix)


if __name__ == "__main__":
    unittest.main()

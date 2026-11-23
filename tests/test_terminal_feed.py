"""Exercise the panel terminal feed ring and flush policy on the host.

The Waveshare panel mirrors the bridge UART through src/linkr_terminal.c: an
ISR-fed ring plus a 100 ms flush that drives LVGL. This test compiles that
production file against small Zephyr shims (see the harness) and pins the
behaviour behind the "after a large burst the panel stops at an old line"
report:

  - a full ring keeps the *newest* bytes, so the last line of a burst is on
    screen;
  - bytes that arrive while a flush runs schedule another flush instead of
    sitting in the ring until unrelated traffic shows up;
  - a flush pushes its rows in one UI batch.
"""
import re
import shlex
import subprocess
import tempfile
import unittest
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class TerminalFeedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="linkr-terminal-feed-")
        cls.addClassCleanup(cls.tmp.cleanup)
        directory = Path(cls.tmp.name)
        # utf-8-sig: the file carries a BOM, and the .inc is included in the
        # middle of the harness where a BOM would be a stray byte.
        production = (ROOT / "src/linkr_terminal.c").read_text(encoding="utf-8-sig")
        # Only the Zephyr includes are removed; the harness supplies the
        # primitives those headers would have provided.
        production = re.sub(r"^#include <zephyr/.*\n", "", production, flags=re.M)
        (directory / "terminal_production.inc").write_text(production, encoding="utf-8")

        cls.binary = directory / "terminal-feed-tests"
        subprocess.run(
            [
                *shlex.split(os.environ.get("CC", "cc")),
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(directory),
                "-I",
                str(ROOT / "src"),
                str(ROOT / "tests/terminal_feed_harness.c"),
                "-o",
                str(cls.binary),
            ],
            check=True,
        )

    def test_feed_policy(self):
        result = subprocess.run(
            [str(self.binary)], capture_output=True, text=True, timeout=30
        )
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertIn("all checks passed", result.stdout)


if __name__ == "__main__":
    unittest.main()

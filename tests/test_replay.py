#!/usr/bin/env python3
"""Checks the PC replay tool against a recording made by test_core.
Run after test_core, from the tests folder: python3 -I test_replay.py"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "tools", "replay.py")
SAMPLE = os.path.join(HERE, "out", "sample", "boot00001-part0001.acr")
failures = 0


def check(cond, msg):
    global failures
    if not cond:
        failures += 1
        print("FAILED: " + msg)


def run(*args):
    return subprocess.run([sys.executable, "-I", TOOL] + list(args), capture_output=True, text=True, timeout=60)


r = run("--info", SAMPLE)
check(r.returncode == 0, "the info option failed: " + r.stderr)
check("30 frames" in r.stdout, "the info text should report 30 frames: " + r.stdout)
check("incomplete frame" in r.stdout, "the info text should mention the torn last frame")
check("no clock" in r.stdout, "the info text should say the start time is unknown")

out = os.path.join(HERE, "out", "export")
shutil.rmtree(out, ignore_errors=True)
r = run("--export", out, SAMPLE)
check(r.returncode == 0, "export failed: " + r.stderr)
files = sorted(os.listdir(out)) if os.path.isdir(out) else []
check(len(files) == 30, "expected 30 exported frames, found %d" % len(files))
if files:
    with open(os.path.join(out, files[0]), encoding="utf-8") as f:
        lines = f.read().splitlines()
    check(len(lines) == 25 and all(len(l) == 80 for l in lines), "an exported frame should be 25 lines of 80 characters")
    check(lines[0].startswith("┌") and lines[0].endswith("┐"), "the test pattern border should start the first line")
    check("TEST PATTERN" in lines[2] and "Frame 00000000" in lines[2], "the first frame should show its counter at zero")
    with open(os.path.join(out, files[-1]), encoding="utf-8") as f:
        check("Frame 00000029" in f.read(), "the last frame should show its counter at 29")

# Playing to a pipe at high speed prints every frame and ends by itself.
r = run("--speed", "100", SAMPLE)
check(r.returncode == 0 and r.stdout.count("TEST PATTERN") == 30, "playback should print all 30 frames")

if failures:
    print("%d replay tool checks failed." % failures)
    sys.exit(1)
print("The replay tool described the recording, exported 30 plain text frames with the right content, ignored the torn last frame, and played all frames at 100 times speed.")

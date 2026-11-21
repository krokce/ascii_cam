#!/usr/bin/env python3
"""Play back a recording from the ASCII camera appliance in a terminal.

Recordings are the files ending in .acr in the recordings folder of the
REC partition. This script needs only Python 3 and its standard library.

Examples, each typed on one line:

  python3 replay.py boot00042-part0003.acr
      Plays the recording at its original speed.

  python3 replay.py --speed 4 boot00042-part0003.acr
      Plays it four times faster.

  python3 replay.py --info boot00042-part0003.acr
      Describes the recording in a few sentences and does not play it.

  python3 replay.py --export frames boot00042-part0003.acr
      Writes every frame as a plain text file into the folder frames.

While playing: the space bar pauses and resumes, the plus key doubles
the speed, the minus key halves it, the n key steps one frame while
paused, and the q key quits. Bright characters are shown in bold.
"""
import argparse
import os
import select
import struct
import sys
import time
import zlib

MAGIC = b"ASCIIREC"
FRAME_MAGIC = b"FRAM"
HEADER_SIZE = 64
MODES = {0: "characters", 1: "shade blocks", 2: "half blocks", 3: "shape matching"}

# Code page 437 characters the appliance uses, as Unicode.
CP437 = {
    0xB0: "░", 0xB1: "▒", 0xB2: "▓", 0xDB: "█",
    0xDC: "▄", 0xDD: "▌", 0xDE: "▐", 0xDF: "▀",
    0xB3: "│", 0xC4: "─", 0xDA: "┌", 0xBF: "┐",
    0xC0: "└", 0xD9: "┘",
}


def char_of(code):
    if 32 <= code <= 126:
        return chr(code)
    return CP437.get(code, "?")


class Recording:
    """Reads the header and every complete, undamaged frame of a file."""

    def __init__(self, path):
        with open(path, "rb") as f:
            data = f.read()
        if len(data) < HEADER_SIZE or data[:8] != MAGIC:
            raise ValueError("%s is not an appliance recording." % path)
        (self.version, hsize, self.cols, self.rows, self.boot, self.part,
         real_lo, real_hi, self.mode, self.charset, self.record_size) = struct.unpack_from(
            "<HHHHIIIIBBH", data, 8)
        if zlib.crc32(data[:60]) != struct.unpack_from("<I", data, 60)[0]:
            raise ValueError("The header of %s is damaged." % path)
        self.real_start_ms = real_lo | (real_hi << 32)
        cells = self.cols * self.rows
        if self.record_size != 4 + 4 + cells + (cells + 7) // 8 + 4:
            raise ValueError("The header of %s gives an unexpected record size." % path)
        self.frames = []
        self.damaged = 0
        off = hsize
        size = self.record_size
        while off + size <= len(data):
            rec = data[off:off + size]
            off += size
            if rec[:4] != FRAME_MAGIC or zlib.crc32(rec[:-4]) != struct.unpack_from("<I", rec, size - 4)[0]:
                self.damaged += 1
                continue
            ms = struct.unpack_from("<I", rec, 4)[0]
            chars = rec[8:8 + cells]
            bits = rec[8 + cells:8 + cells + (cells + 7) // 8]
            bright = [(bits[i // 8] >> (i % 8)) & 1 for i in range(cells)]
            self.frames.append((ms, chars, bright))
        self.torn_bytes = len(data) - off

    def lines(self, index, bold=False):
        ms, chars, bright = self.frames[index]
        out = []
        for r in range(self.rows):
            parts = []
            cur = 0
            for c in range(self.cols):
                i = r * self.cols + c
                if bold and bright[i] != cur:
                    parts.append("\x1b[1m" if bright[i] else "\x1b[22m")
                    cur = bright[i]
                parts.append(char_of(chars[i]))
            if bold and cur:
                parts.append("\x1b[22m")
            out.append("".join(parts))
        return out

    def describe(self):
        n = len(self.frames)
        dur = self.frames[-1][0] / 1000.0 if n else 0.0
        s = ["This recording is part %d of boot %d." % (self.part, self.boot),
             "It has %d frames covering %.1f seconds, drawn in %s mode on a grid of %d by %d characters."
             % (n, dur, MODES.get(self.mode, "an unknown"), self.cols, self.rows)]
        if self.real_start_ms:
            s.append("It started on %s." % time.strftime("%d %B %Y at %H:%M:%S", time.localtime(self.real_start_ms / 1000)))
        else:
            s.append("The real start time is not known, because the appliance had no clock.")
        if self.damaged:
            s.append("%d damaged frames were skipped." % self.damaged)
        if self.torn_bytes:
            s.append("The end of the file holds an incomplete frame, probably from a power cut, which was ignored.")
        return " ".join(s)


def export(rec, folder):
    os.makedirs(folder, exist_ok=True)
    for i in range(len(rec.frames)):
        name = os.path.join(folder, "frame%06d.txt" % (i + 1))
        with open(name, "w", encoding="utf-8") as f:
            f.write("\n".join(rec.lines(i)) + "\n")
    print("Exported %d frames as plain text files into the folder %s." % (len(rec.frames), folder))


def play(rec, speed):
    interactive = sys.stdin.isatty() and sys.stdout.isatty()
    old = None
    if interactive:
        import termios
        import tty
        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        tty.setcbreak(fd)
        sys.stdout.write("\x1b[?25l\x1b[2J")
    try:
        i = 0
        paused = False
        clock = time.monotonic()
        base = rec.frames[0][0] if rec.frames else 0
        while i < len(rec.frames):
            ms = rec.frames[i][0]
            due = clock + (ms - base) / 1000.0 / speed
            step = False
            while True:
                wait = 0.05 if paused else max(0.0, due - time.monotonic())
                if interactive:
                    ready, _, _ = select.select([sys.stdin], [], [], min(wait, 0.05))
                    if ready:
                        key = sys.stdin.read(1)
                        if key == "q":
                            return
                        if key == " ":
                            paused = not paused
                            # Restart the clock from this frame.
                            clock = time.monotonic()
                            base = ms
                            due = clock
                        elif key == "+":
                            speed *= 2
                            clock, base, due = time.monotonic(), ms, time.monotonic()
                        elif key == "-":
                            speed /= 2
                            clock, base, due = time.monotonic(), ms, time.monotonic()
                        elif key == "n" and paused:
                            step = True
                elif wait > 0:
                    time.sleep(wait)
                if step or (not paused and time.monotonic() >= due):
                    break
            lines = rec.lines(i, bold=interactive)
            if interactive:
                status = "Frame %d of %d, %.1f seconds, speed %g%s. Space pauses, plus and minus change speed, q quits." % (
                    i + 1, len(rec.frames), ms / 1000.0, speed, ", paused" if paused else "")
                sys.stdout.write("\x1b[H" + "\n".join(lines) + "\n\x1b[K" + status)
            else:
                sys.stdout.write("\n".join(lines) + "\n\n")
            sys.stdout.flush()
            i += 1
    finally:
        if interactive:
            import termios
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old)
            sys.stdout.write("\x1b[?25h\n")


def main():
    p = argparse.ArgumentParser(description="Play back an ASCII camera recording in a terminal.")
    p.add_argument("file", help="the recording, a file ending in .acr")
    p.add_argument("--speed", type=float, default=1.0, help="playback speed, 1 is the original speed")
    p.add_argument("--export", metavar="FOLDER", help="write each frame as a plain text file into FOLDER")
    p.add_argument("--info", action="store_true", help="describe the recording in words and do not play it")
    a = p.parse_args()
    try:
        rec = Recording(a.file)
    except (OSError, ValueError) as e:
        print("The recording could not be read: %s" % e, file=sys.stderr)
        return 1
    if a.info:
        print(rec.describe())
        return 0
    if a.export:
        export(rec, a.export)
        return 0
    if not rec.frames:
        print("The recording holds no complete frames.")
        return 0
    if a.speed <= 0:
        print("The speed must be greater than zero.", file=sys.stderr)
        return 1
    try:
        play(rec, a.speed)
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())

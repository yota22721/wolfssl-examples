#!/usr/bin/env python3
"""Behavioral test driver for the interactive PUF demo (host build).

Drives the menu over stdin/stdout and asserts on the demo's output markers:
enrollment, the sweep gate and correction cliff, blob dump and recovery,
checksum rejection, identity-mismatch rejection, paste abort, and the
fail-closed unhealthy-readout path. Exits nonzero on the first failure.
"""
import os
import re
import select
import subprocess
import sys
import time

BIN = sys.argv[1] if len(sys.argv) > 1 else "./puf_host_test"
TIMEOUT = 15


class Demo:
    def __init__(self, env=None):
        e = dict(os.environ)
        if env:
            e.update(env)
        self.p = subprocess.Popen([BIN], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, env=e)
        self.buf = b""

    def send(self, text):
        self.p.stdin.write(text.encode())
        self.p.stdin.flush()

    def expect(self, *patterns):
        """Read until every pattern has appeared (in the stream so far)."""
        deadline = time.time() + TIMEOUT
        remaining = list(patterns)
        while remaining:
            remaining = [p for p in remaining
                         if not re.search(p.encode(), self.buf)]
            if not remaining:
                break
            if time.time() > deadline:
                raise AssertionError(
                    "timeout waiting for %r; got:\n%s" %
                    (remaining, self.buf.decode(errors="replace")[-2000:]))
            r, _, _ = select.select([self.p.stdout], [], [], 0.2)
            if r:
                chunk = os.read(self.p.stdout.fileno(), 65536)
                if not chunk:
                    raise AssertionError(
                        "EOF waiting for %r; got:\n%s" %
                        (remaining, self.buf.decode(errors="replace")[-2000:]))
                self.buf += chunk

    def absent(self, pattern):
        if re.search(pattern.encode(), self.buf):
            raise AssertionError("unexpected %r in:\n%s" %
                                 (pattern, self.buf.decode(errors="replace")))

    def clear(self):
        self.buf = b""

    def close(self):
        self.p.stdin.close()
        try:
            self.p.wait(timeout=TIMEOUT)
        finally:
            if self.p.poll() is None:
                self.p.kill()


def checksum(data):
    s = 0xFFFF
    for b in data:
        s = ((s << 5) ^ (s >> 11) ^ b) & 0xFFFF
    return s


def healthy_run():
    d = Demo()
    d.expect(r"interactive demo", r"inside the health band")
    d.absent(r"synthetic")

    # sweep is gated before an enrollment from this boot
    d.clear()
    d.send("2")
    d.expect(r"run \[1\] enroll first")

    # enroll: identity shown, key NOT shown by default
    d.clear()
    d.send("1")
    d.expect(r"enrolled from this boot", r"identity    : [0-9a-f]{32}",
             r"derived OK \(not shown")
    d.absent(r"derived key : [0-9a-f]{32}")

    # sweep: full correction cliff, never the wrong key
    d.clear()
    d.send("2")
    d.expect(r"<= t, the limit", r"rejected \(-\d+\) - fails closed")
    d.absent(r"WRONG KEY")

    # two keys: derivation succeeds, no key material on the wire
    d.clear()
    d.send("3")
    d.expect(r"two HKDF contexts", r"derived OK \(not shown")
    d.absent(r"[0-9a-f]{32}\r")

    # dump the recovery blob (id + helper + 2-byte checksum, one hex line)
    d.clear()
    d.send("4")
    d.expect(r"\r\n[0-9a-f]{300,}\r\n")
    blob = re.search(rb"\r\n([0-9a-f]{300,})\r\n", d.buf).group(1).decode()
    raw = bytes.fromhex(blob)
    assert checksum(raw[:-2]) == int.from_bytes(raw[-2:], "big"), \
        "dumped blob checksum does not verify"

    # paste it back: checksum OK, same key
    d.clear()
    d.send("5")
    d.expect(r"paste the recovery blob")
    d.send(blob)
    d.expect(r"checksum [0-9a-f]{4} OK", r"SAME KEY")

    # sweep gated again after a loaded blob
    d.clear()
    d.send("2")
    d.expect(r"run \[1\] enroll first")

    # mangled checksum: rejected, nothing changed
    d.clear()
    d.send("5")
    d.expect(r"paste the recovery blob")
    bad = blob[:-1] + ("0" if blob[-1] != "0" else "1")
    d.send(bad)
    d.expect(r"checksum mismatch", r"nothing was changed")

    # corrupted identity with a recomputed valid checksum: MISMATCH, no commit
    body = bytearray(raw[:-2])
    body[0] ^= 0x01
    wrong = body.hex() + format(checksum(body), "04x")
    d.clear()
    d.send("5")
    d.expect(r"paste the recovery blob")
    d.send(wrong)
    d.expect(r"MISMATCH - this blob does not belong",
             r"nothing was changed")

    # truncated paste + q: aborted
    d.clear()
    d.send("5")
    d.expect(r"paste the recovery blob")
    d.send(blob[:40] + "q")
    d.expect(r"aborted")

    d.close()
    print("healthy-path scenarios: PASS")


def unhealthy_run():
    d = Demo(env={"PUF_HOST_UNHEALTHY": "1"})
    d.expect(r"REJECTED by the health band",
             r"enrollment and key derivation are disabled")
    d.absent(r"synthetic")
    for opt in "135":
        d.clear()
        d.send(opt)
        # option 3 is additionally gated on enrollment; either refusal is a
        # correct fail-closed response
        d.expect(r"(failed the health check|run \[1\] enroll first)")
        d.absent(r"[0-9a-f]{32}")
    d.close()
    print("unhealthy fail-closed scenarios: PASS")


def main():
    healthy_run()
    unhealthy_run()
    print("ALL PASS")


if __name__ == "__main__":
    main()

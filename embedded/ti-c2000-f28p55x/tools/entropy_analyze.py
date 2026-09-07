#!/usr/bin/env python3
"""Analyze the ENTROPY_PROBE=1 capture from the TI C2000 (C28x) entropy probe.

Consumes the tagged lines that Source/entropy_probe.c emits over the SCI
console and reports, per noise source, the four figures published in the port
README: min-entropy per bit, per-bit bias, peak autocorrelation over lags
1..64, and a chi-square uniformity p-value.

Tags produced by the probe:
  E0 <window> <count> ...   raw DCC1 count, INTOSC1 window / PLL counted
  E1 <window> <count> ...   raw DCC0 count, INTOSC2 window / PLL counted
  E3 0 <result> ...         raw ADC result, floating input
  E4 256 <octet> ...        packed LSB stream, INTOSC1  (the credited source)
  E5 256 <octet> ...        packed LSB stream, INTOSC2
  E6 0 <octet> ...          packed LSB stream, ADC

The E4/E5/E6 streams are what the analysis uses: the bit extraction happens
on-target (8 samples per emitted octet) because a useful min-entropy estimate
needs far more samples than the UART can carry one hex count at a time, and
because that packed stream is exactly what the entropy source consumes.

Method.  Min-entropy is the SP800-90B 6.3.1 most-common-value estimate taken
over the 8-bit octet alphabet at the 99% upper confidence bound, divided by 8
to express it per bit; the octet alphabet is used rather than the bit alphabet
because it also catches structure across adjacent bits, which a per-bit
estimate cannot see.  This is an MCV estimate plus bias and correlation
screening, NOT a full SP800-90B non-IID assessment: MCV assumes IID, so it is
an upper bound, and low measured correlation is what makes it a reasonable one.

Usage:
    python3 tools/entropy_analyze.py capture.log
    python3 tools/entropy_analyze.py --selftest
"""

import math
import re
import sys
from collections import OrderedDict

try:
    import numpy as np
except ImportError:
    sys.exit("numpy is required: pip install numpy")

# Packed-LSB streams, in report order, with the labels the README table uses.
PACKED = OrderedDict((
    ("E4", "INTOSC1 window / PLL counted (DCC1)"),
    ("E5", "INTOSC2 window / PLL counted (DCC0)"),
    ("E6", "ADC LSB, floating input"),
))
RAW = OrderedDict((
    ("E0", "INTOSC1 raw DCC count"),
    ("E1", "INTOSC2 raw DCC count"),
    ("E3", "ADC raw result"),
))

BANNER = "=== ENTROPY PROBE ==="
# The probe reports how many samples were taken after a DCC ERROR flag or a
# guard-loop timeout.  Any nonzero count means the capture is not pure noise.
HEALTH_RE = re.compile(r"DCC errors (\d+), DCC timeouts (\d+), "
                       r"ADC timeouts (\d+)")
DONE = "PROBE DONE"
MAX_LAG = 64
# Below this the MCV bound is too wide to be worth reporting.
MIN_PACKED_OCTETS = 256
Z_99 = 2.5758293035489004          # two-sided 99% normal quantile


def first_pass(text):
    """Return just the first complete probe pass.  The probe image runs once
    and then spins, so a normal capture holds one pass - but a capture that
    spans a reset or a re-flash would otherwise concatenate several runs into
    one sample set."""
    start = text.find(BANNER)
    if start < 0:
        return text
    end = text.find(DONE, start)
    if end < 0:
        sys.stderr.write("warning: no '%s' marker - capture may be truncated\n"
                         % DONE)
        return text[start:]
    return text[start:end]


def parse(text):
    """Returns (out, per_window).

    out        tag -> list of ints, in emission order (all windows merged).
    per_window tag -> {window: [ints]}.

    The raw DCC tags are emitted once per sweep window (256/1024/4096), and a
    count scales with the window, so merging them would mix populations of
    different magnitude.  The packed streams the min-entropy estimate uses are
    emitted once, at a single window, so out[] is correct for those."""
    out = {}
    per_window = {}
    # A console line is "<tag> <window> <hex> <hex> ...".  Match per line, and
    # never across a newline: the tags and window counts are themselves valid
    # hex, so a multi-line match would swallow the next line's header as data.
    # Tolerate any timestamp or prefix a log wrapper put ahead of the tag.
    line_re = re.compile(r"\b(E[0-9])[^\S\n]+(\d+)[^\S\n]+"
                         r"((?:[0-9a-fA-F]+[^\S\n]*)+)$")
    skipped = 0
    for line in text.splitlines():
        line = line.rstrip()
        m = line_re.search(line)
        if m is None:
            # A line that starts with a probe tag but does not parse means a
            # corrupted capture, not unrelated console output - say so.
            if re.match(r"\s*E[0-9]\b", line):
                skipped += 1
            continue
        vals = [int(t, 16) for t in m.group(3).split()]
        out.setdefault(m.group(1), []).extend(vals)
        per_window.setdefault(m.group(1), {}).setdefault(
            int(m.group(2)), []).extend(vals)
    if skipped:
        sys.stderr.write("warning: %d probe line(s) did not parse - capture may "
                         "be corrupted or truncated\n" % skipped)
    return out, per_window


def unpack_bits(octets):
    """Octets back to the LSB-first bit stream the probe packed."""
    a = np.asarray(octets, dtype=np.uint8)
    return np.unpackbits(a[:, None], axis=1, bitorder="little").ravel()


def mcv_min_entropy(symbols, alphabet):
    """SP800-90B 6.3.1 most-common-value estimate, 99% upper bound, in bits
    per symbol."""
    n = len(symbols)
    if n < 2:
        return float("nan")
    counts = np.bincount(np.asarray(symbols, dtype=np.int64),
                         minlength=alphabet)
    p_hat = counts.max() / n
    p_u = min(1.0, p_hat + Z_99 * math.sqrt(p_hat * (1.0 - p_hat) / (n - 1)))
    return -math.log2(p_u)


def max_abs_acf(bits, max_lag=MAX_LAG):
    """Peak |autocorrelation| over lags 1..max_lag of the bit stream."""
    x = np.asarray(bits, dtype=np.float64)
    x = x - x.mean()
    denom = float(np.dot(x, x))
    if denom == 0.0:
        # Constant stream: no correlation is defined.  Return the same
        # (value, lag) shape callers unpack - a stuck source is exactly the
        # case that must report cleanly rather than raise.
        return float("nan"), 0
    peak, at = 0.0, 0
    for lag in range(1, min(max_lag, len(x) - 1) + 1):
        r = abs(float(np.dot(x[:-lag], x[lag:])) / denom)
        if r > peak:
            peak, at = r, lag
    return peak, at


def _gamma_q(s, x):
    """Regularized upper incomplete gamma Q(s,x), by the series for P(s,x)
    when x < s+1 and Lentz's continued fraction for Q(s,x) otherwise.  Written
    out because scipy is not assumed present and the Wilson-Hilferty
    approximation, while fine in the tails, is off by ~0.01 near the median -
    and a uniformity p-value in the middle of the range is exactly what gets
    published."""
    if x < 0.0 or s <= 0.0:
        return float("nan")
    if x == 0.0:
        return 1.0

    if x < s + 1.0:                       # series for P(s,x), Q = 1 - P
        term = 1.0 / s
        total = term
        n = s
        for _ in range(1000):
            n += 1.0
            term *= x / n
            total += term
            if abs(term) < abs(total) * 1e-16:
                break
        return 1.0 - total * math.exp(-x + s * math.log(x) - math.lgamma(s))

    tiny = 1e-300                         # continued fraction for Q(s,x)
    b = x + 1.0 - s
    c = 1.0 / tiny
    d = 1.0 / b
    h = d
    for i in range(1, 1000):
        an = -i * (i - s)
        b += 2.0
        d = an * d + b
        if abs(d) < tiny:
            d = tiny
        c = b + an / c
        if abs(c) < tiny:
            c = tiny
        d = 1.0 / d
        delta = d * c
        h *= delta
        if abs(delta - 1.0) < 1e-16:
            break
    return h * math.exp(-x + s * math.log(x) - math.lgamma(s))


def chi2_sf(x, k):
    """Upper tail of chi-square(k)."""
    if k <= 0:
        return float("nan")
    return _gamma_q(k / 2.0, x / 2.0)


def chi2_uniform_octets(octets):
    """Chi-square goodness of fit of the octet histogram against uniform."""
    n = len(octets)
    counts = np.bincount(np.asarray(octets, dtype=np.int64), minlength=256)
    expected = n / 256.0
    stat = float(((counts - expected) ** 2 / expected).sum())
    return stat, chi2_sf(stat, 255)


def report(tag, label, octets):
    bits = unpack_bits(octets)
    n_bits = len(bits)
    ones = int(bits.sum())
    bias = ones / n_bits - 0.5

    h_octet = mcv_min_entropy(octets, 256)
    h_per_bit = h_octet / 8.0
    h_bitwise = mcv_min_entropy(bits, 2)
    acf, acf_lag = max_abs_acf(bits)
    chi_stat, chi_p = chi2_uniform_octets(octets)

    print("%s  %s" % (tag, label))
    print("    samples          %d octets (%d bits)" % (len(octets), n_bits))
    print("    Hmin/bit         %.3f   (octet MCV %.3f bits / 8)"
          % (h_per_bit, h_octet))
    print("    Hmin/bit bitwise %.3f   (bit-alphabet MCV, less conservative)"
          % h_bitwise)
    print("    bias             %.4f  (%d/%d ones)" % (bias, ones, n_bits))
    print("    max |acf| 1..%-3d %.3f   (at lag %d)" % (MAX_LAG, acf, acf_lag))
    print("    chi-square p     %.3f   (stat %.1f, df 255)"
          % (chi_p, chi_stat))
    print()
    return dict(tag=tag, label=label, h=h_per_bit, bias=bias, acf=acf,
                p=chi_p)


def selftest():
    """Synthetic streams with known properties, so the estimators can be
    trusted before they are pointed at real silicon.  Also calibrates the
    ceiling: at this sample count the octet-MCV/8 estimate of a genuinely
    uniform stream lands near 0.93, not 1.0, so a measured 0.92 is at the
    estimator's practical maximum rather than 8% short of ideal."""
    # chi2_sf is hand-rolled (no scipy), so check it against known quantiles
    # before anything relies on the p-values it produces.
    known = [(1.0, 1, 0.317311), (10.0, 10, 0.440493),
             (3.841459, 1, 0.05), (18.307038, 10, 0.05),
             (293.2478, 255, 0.05), (310.4574, 255, 0.01),
             (284.3359, 255, 0.10)]
    worst = max(abs(chi2_sf(x, k) - want) for x, k, want in known)
    print("chi2_sf worst error vs known quantiles: %.2e  %s\n"
          % (worst, "OK" if worst < 2e-4 else "FAIL"))

    rng = np.random.default_rng(1234)
    n = 32768
    print("Estimator self-test (%d octets per case)\n" % n)

    uniform = rng.integers(0, 256, n)
    report("--", "uniform (ceiling: Hmin ~0.93, p uniform, acf ~0)", uniform)

    bits = (rng.random(n * 8) < 0.55).astype(np.uint8)
    report("--", "biased p(1)=0.55 (expect lower Hmin, bias ~0.05, p=0)",
           np.packbits(bits.reshape(-1, 8), axis=1, bitorder="little").ravel())

    x = np.zeros(n * 8, dtype=np.uint8)
    for i in range(1, len(x)):
        x[i] = x[i - 1] if rng.random() < 0.85 else 1 - x[i - 1]
    report("--", "lag-1 correlated, unbiased marginal "
                 "(bitwise MCV is blind to this; octet MCV and acf are not)",
           np.packbits(x.reshape(-1, 8), axis=1, bitorder="little").ravel())


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "-"
    if path == "--selftest":
        selftest()
        return
    if path == "-":
        text = sys.stdin.read()
    else:
        with open(path, errors="replace") as f:
            text = f.read()

    health = HEALTH_RE.search(text)
    if health is None:
        sys.stderr.write("warning: no probe health line - old probe image, or "
                         "the capture is truncated\n")
    elif any(int(g) for g in health.groups()):
        sys.exit("probe reported %s DCC errors, %s DCC timeouts, %s ADC "
                 "timeouts - these samples are not noise, reject the capture"
                 % health.groups())

    data, per_window = parse(first_pass(text))
    if not data:
        sys.exit("no probe tags found - is this an ENTROPY_PROBE=1 capture?")

    rows = []
    for tag, label in PACKED.items():
        octets = data.get(tag)
        if not octets:
            sys.stderr.write("warning: no %s samples (%s)\n" % (tag, label))
            continue
        if len(octets) < MIN_PACKED_OCTETS:
            sys.stderr.write("warning: %s has only %d octets (< %d) - the "
                             "min-entropy estimate will be unreliable\n"
                             % (tag, len(octets), MIN_PACKED_OCTETS))
        bad = [v for v in octets if v > 0xFF]
        if bad:
            sys.exit("%s: %d values exceed one octet - capture is corrupt"
                     % (tag, len(bad)))
        rows.append(report(tag, label, octets))

    for tag, label in RAW.items():
        wins = per_window.get(tag)
        if not wins:
            continue
        # Per window: a raw count scales with the window, so pooling them
        # would report a spread that is an artifact of the sweep.
        for win in sorted(wins):
            a = np.asarray(wins[win], dtype=np.int64)
            print("%s  %s [window %d]: %d samples, min %d max %d mean %.1f, "
                  "%d distinct" % (tag, label, win, len(a), a.min(), a.max(),
                                   a.mean(), len(np.unique(a))))
    print()

    print("README table:")
    print()
    print("| Source | Hmin/bit | bias | max \\|acf\\| lag 1..%d | chi-square p |"
          % MAX_LAG)
    print("|---|---|---|---|---|")
    for r in rows:
        print("| %s | %.3f | %.4f | %.3f | %.3f |"
              % (r["label"], r["h"], r["bias"], r["acf"], r["p"]))


if __name__ == "__main__":
    main()

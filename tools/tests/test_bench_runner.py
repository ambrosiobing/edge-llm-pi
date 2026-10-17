"""The parsing and the gate of tools/bench_runner.py, checked against a real run.

The fixtures below are the output of e01's first measured run on the bench Raspberry Pi 4
Model B on Friday 16 October 2026, cut down to the rows that matter. The expected
percentages were worked out by hand from that run before this parser existed, so these
tests compare the code against arithmetic done independently of it rather than against
itself.

Why they exist at all: the runner now decides pass or fail, and a measuring tool that
decides anything has to be measured itself. Nothing here needs a Raspberry Pi, a model
file or a compiler.
"""
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import bench_runner  # noqa: E402

REFERENCE = """
| model                  |       size |     params | backend | threads |  test |            t/s |
| ---------------------- | ---------: | ---------: | ------- | ------: | ----: | -------------: |
| qwen2 1B Q4_K - Medium | 462.96 MiB |   630.17 M | CPU     |       3 |  pp64 | 20.04 +/- 0.01 |
| qwen2 1B Q4_K - Medium | 462.96 MiB |   630.17 M | CPU     |       3 |  tg32 |  9.47 +/- 0.01 |
| qwen2 1B Q4_K - Medium | 462.96 MiB |   630.17 M | CPU     |       4 |  pp64 | 25.58 +/- 1.57 |
| qwen2 1B Q4_K - Medium | 462.96 MiB |   630.17 M | CPU     |       4 |  tg32 |  8.70 +/- 1.09 |

build: d81235049 (11429)
"""

HARNESS = """
| model | size | params | backend | threads | test | t/s |
| ----- | ---- | ------ | ------- | ------- | ---- | --- |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 3 | pp 64 | 19.99 +/- 0.01 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 3 | tg 32 | 9.26 +/- 0.01 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 4 | pp 64 | 24.70 +/- 1.74 |
| qwen2 1B Q4_K - Medium | 463 MiB | 0.63 B | CPU | 4 | tg 32 | 9.18 +/- 0.13 |

harness: qwen2.5-0.5b-instruct-q4_k_m.gguf, 5 repeats, prompt file prompt.txt
VmRSS kB 542648
system: CPU : NEON = 1 | ARM_FMA = 1 | LLAMAFILE = 1 | OPENMP = 1 | REPACK = 1 |
"""


def test_both_table_dialects_parse():
    """The reference writes pp64, the harness writes "pp 64"; they must key alike."""
    reference = bench_runner.parse_rows(REFERENCE)
    harness = bench_runner.parse_rows(HARNESS)
    assert sorted(reference) == sorted(harness)
    assert sorted(reference) == [(3, "pp64"), (3, "tg32"), (4, "pp64"), (4, "tg32")]


def test_the_spread_is_kept_not_discarded():
    """The four-thread spread is why the gate sits at three; losing it loses the reason."""
    reference = bench_runner.parse_rows(REFERENCE)
    assert reference[(4, "tg32")] == (8.70, 1.09)
    assert reference[(3, "tg32")] == (9.47, 0.01)


def test_the_system_line_is_not_read_as_a_row():
    """The harness prints a "system:" line full of pipe characters after its table."""
    rows = bench_runner.parse_rows(HARNESS)
    assert all(isinstance(t, int) for t, _ in rows)
    assert len(rows) == 4


def test_division_matches_the_arithmetic_done_by_hand():
    reference = bench_runner.parse_rows(REFERENCE)
    harness = bench_runner.parse_rows(HARNESS)
    got = {(t, test): round(pct, 2)
           for t, test, _, _, pct in bench_runner.divide(reference, harness)}
    assert got == {(3, "pp64"): -0.25, (3, "tg32"): -2.22,
                   (4, "pp64"): -3.44, (4, "tg32"): 5.52}


def test_the_first_real_run_passes_at_three_threads():
    reference = bench_runner.parse_rows(REFERENCE)
    harness = bench_runner.parse_rows(HARNESS)
    failed, text = bench_runner.gate(reference, harness, 3, 10.0)
    assert not failed
    assert text.startswith("PASS")


def test_a_harness_that_disagrees_fails():
    """The known positive. A gate that has never refused anything is not known to work."""
    reference = bench_runner.parse_rows(REFERENCE)
    harness = dict(bench_runner.parse_rows(HARNESS))
    harness[(3, "tg32")] = (9.47 * 1.2, 0.01)
    failed, text = bench_runner.gate(reference, harness, 3, 10.0)
    assert failed
    assert text.startswith("FAIL")


def test_a_missing_figure_fails_rather_than_passes():
    """A gate that cannot be applied has not been satisfied."""
    reference = bench_runner.parse_rows(REFERENCE)
    harness = dict(bench_runner.parse_rows(HARNESS))
    del harness[(3, "tg32")]
    failed, text = bench_runner.gate(reference, harness, 3, 10.0)
    assert failed
    assert "REFUSED" in text


def test_no_harness_is_not_a_failure():
    failed, text = bench_runner.gate({}, {}, 3, 10.0, ran=False)
    assert not failed
    assert "not run" in text


@pytest.mark.parametrize("band,expected_fail", [(10.0, False), (2.0, True)])
def test_the_band_is_the_band(band, expected_fail):
    """2.22 percent passes a band of 10 and fails a band of 2; the number is used."""
    reference = bench_runner.parse_rows(REFERENCE)
    harness = bench_runner.parse_rows(HARNESS)
    failed, _ = bench_runner.gate(reference, harness, 3, band)
    assert failed is expected_fail


def test_decode_reads_the_value_this_bench_actually_produced():
    """0x50000 is what the Pi 4B reported three times on Saturday 17 October 2026."""
    words = bench_runner.decode("throttled=0x50000")
    assert words == "under-voltage has occurred, throttling has occurred"


def test_decode_calls_a_clean_register_clean():
    assert bench_runner.decode("throttled=0x0") == "clean"


def test_decode_separates_now_from_has_occurred():
    """The low bits mean it is happening; the high bits mean it happened since boot."""
    assert bench_runner.decode("throttled=0x1") == "under-voltage now"
    assert bench_runner.decode("throttled=0x10000") == "under-voltage has occurred"


def test_decode_names_the_soft_temperature_limit_separately():
    """Bit 19 was never set in the refusals, which is how we knew it was not heat."""
    assert bench_runner.decode("throttled=0x80000") == "soft temperature limit has occurred"


def test_decode_passes_through_what_it_cannot_read():
    """A host without vcgencmd records "not checked", and that must survive unchanged."""
    assert bench_runner.decode("not checked") == "not checked"
    assert bench_runner.decode("throttled=0xzz") == "throttled=0xzz"

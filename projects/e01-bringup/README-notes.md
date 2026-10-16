# e01 notes: why there is no replay folder

Every other project folder carries `replay/*.csv`, the event stream the loop reads with
`--replay` on the host and in CI, so that the bench run is the same binary with `--live`.

Project e01 has no event stream. It decides nothing and reads no sensor: it is the
instrument that measures the engine on each board, and its input is the model file and
the 64 tokens of `prompt.txt`. There is nothing a CSV could stand in for. What takes the
place of the oracle is numeric: the harness figure must sit within 10 percent of the
upstream `llama-bench` figure on the same board, model and thread count, and the throttle
bits must read `0x0` before and after. `tools/bench_runner.py` runs the reference with the
gate and files it under `evidence/`; the harness run is pasted into the same file by hand,
and the comparison is one division written beside it.

The harness is not built in the stub CI build, because a stub engine has no tokens per
second to report. Its first build is on the boards themselves with `-DGGML_NATIVE=ON`, or
for the NanoPi NEO Air in skyhorizon's WSL with the armv7 cross toolchain. The engine tag
is the one in `cmake/llama_tag.cmake`, `v0.6.0`.

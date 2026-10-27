# llama-bench on pi3bplus-rpi3bv1p2, Tuesday 27 October 2026, 19:41

- model: SmolLM2-360M-Instruct-Q4_K_M.gguf
- kernel: Linux 6.18.50+rpt-rpi-v8 aarch64
- command: build/bin/llama-bench -m /home/bing/models/SmolLM2-360M-Instruct-Q4_K_M.gguf -p 64 -n 32 -t 1,2,3,4 -r 5 -o md
- header: bare header, no HAT
- settling pause between stages: 60 s
- instrument: upstream llama-bench; the volume's harness is priced against this table and published only within 10.0 percent of it

**REFUSED at the reference.** See the verdict below. The figures in this file are kept so the failure can be read; none of them is publishable.

## The register, read at every stage

| moment | vcgencmd get_throttled | in words | temperature |
|---|---|---|---|
| before any load | throttled=0x0 | clean | temp=36.5'C |
| after the reference | throttled=0x80008 | soft temperature limit now, soft temperature limit has occurred | temp=68.2'C |

Read between stages and not only at the ends, because a run refused at the end says the rail sagged without saying when.

| model                          |       size |     params | backend    | threads |            test |                  t/s |
| ------------------------------ | ---------: | ---------: | ---------- | ------: | --------------: | -------------------: |
| llama 3B Q4_K - Medium         | 256.35 MiB |   361.82 M | CPU        |       1 |            pp64 |          3.39 +/- 0.00 |
| llama 3B Q4_K - Medium         | 256.35 MiB |   361.82 M | CPU        |       1 |            tg32 |          2.76 +/- 0.00 |
| llama 3B Q4_K - Medium         | 256.35 MiB |   361.82 M | CPU        |       2 |            pp64 |          6.70 +/- 0.00 |
| llama 3B Q4_K - Medium         | 256.35 MiB |   361.82 M | CPU        |       2 |            tg32 |          5.10 +/- 0.06 |
| llama 3B Q4_K - Medium         | 256.35 MiB |   361.82 M | CPU        |       3 |            pp64 |          8.57 +/- 0.06 |
| llama 3B Q4_K - Medium         | 256.35 MiB |   361.82 M | CPU        |       3 |            tg32 |          6.11 +/- 0.00 |
| llama 3B Q4_K - Medium         | 256.35 MiB |   361.82 M | CPU        |       4 |            pp64 |         11.19 +/- 0.00 |
| llama 3B Q4_K - Medium         | 256.35 MiB |   361.82 M | CPU        |       4 |            tg32 |          7.24 +/- 0.04 |

build: d81235049 (11429)

**Verdict: REFUSED at the reference: the register read throttled=0x80008 (soft temperature limit now, soft temperature limit has occurred). The stages after it were not run. Everything above was measured before the register went dirty and is kept so the failure can be read, not so it can be quoted..**

# llama-bench on pi4b-eplepi, Tuesday 27 October 2026, 19:58

- model: tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
- kernel: Linux 6.18.50+rpt-rpi-v8 aarch64
- command: build/bin/llama-bench -m /home/bing/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf -p 64 -n 32 -t 1,2,3,4 -r 5 -o md
- header: bare header, no HAT
- settling pause between stages: 60 s
- instrument: upstream llama-bench; the volume's harness is priced against this table and published only within 10.0 percent of it

**REFUSED at the reference.** See the verdict below. The figures in this file are kept so the failure can be read; none of them is publishable.

## The register, read at every stage

| moment | vcgencmd get_throttled | in words | temperature |
|---|---|---|---|
| before any load | throttled=0x0 | clean | temp=44.3'C |
| after the reference | throttled=0x80000 | soft temperature limit has occurred | temp=79.3'C |

Read between stages and not only at the ends, because a run refused at the end says the rail sagged without saying when.

| model                          |       size |     params | backend    | threads |            test |                  t/s |
| ------------------------------ | ---------: | ---------: | ---------- | ------: | --------------: | -------------------: |
| llama 1B Q4_K - Medium         | 636.18 MiB |     1.10 B | CPU        |       1 |            pp64 |          3.31 +/- 0.00 |
| llama 1B Q4_K - Medium         | 636.18 MiB |     1.10 B | CPU        |       1 |            tg32 |          2.86 +/- 0.00 |
| llama 1B Q4_K - Medium         | 636.18 MiB |     1.10 B | CPU        |       2 |            pp64 |          6.61 +/- 0.00 |
| llama 1B Q4_K - Medium         | 636.18 MiB |     1.10 B | CPU        |       2 |            tg32 |          5.54 +/- 0.00 |
| llama 1B Q4_K - Medium         | 636.18 MiB |     1.10 B | CPU        |       3 |            pp64 |          9.78 +/- 0.00 |
| llama 1B Q4_K - Medium         | 636.18 MiB |     1.10 B | CPU        |       3 |            tg32 |          5.95 +/- 0.00 |
| llama 1B Q4_K - Medium         | 636.18 MiB |     1.10 B | CPU        |       4 |            pp64 |         12.43 +/- 0.24 |
| llama 1B Q4_K - Medium         | 636.18 MiB |     1.10 B | CPU        |       4 |            tg32 |          5.69 +/- 0.00 |

build: d81235049 (11429)

**Verdict: REFUSED at the reference: the register read throttled=0x80000 (soft temperature limit has occurred). The stages after it were not run. Everything above was measured before the register went dirty and is kept so the failure can be read, not so it can be quoted..**

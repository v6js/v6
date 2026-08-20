# WASM/WASI benchmarks

Release build (`make BUILD_TYPE=release`), no AOT. `cli/*` benchmarks run the
`.wasm` file directly (`wasmtime run file.wasm` vs `v6 file.wasm`, WASI fully
enabled by default on both). `js/*` benchmarks load the same `.wasm` via the
`WebAssembly` API from a JS driver (`node file.js` vs `v6 file.js`, v6 measured
with its normal daemon-accelerated invocation, matching how `bench/report.md`
already measures it). hyperfine, 6 warmups, 5+ min runs per comparison.

## cli/fib.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 51.2 ± 3.9 | 48.3 | 68.2 | 1.00 |
| `v6 v0.1.0` | 262.5 ± 13.5 | 251.1 | 301.0 | 5.13 ± 0.47 |

## cli/primes.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 30.5 ± 1.1 | 27.4 | 35.1 | 1.00 |
| `v6 v0.1.0` | 248.3 ± 5.5 | 240.4 | 259.0 | 8.15 ± 0.35 |

## cli/wasi_io.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 202.8 ± 30.9 | 161.7 | 256.0 | 1.00 |
| `v6 v0.1.0` | 276.4 ± 11.3 | 260.2 | 298.7 | 1.36 ± 0.22 |

## js/fib.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 85.0 ± 4.5 | 77.8 | 95.5 | 1.72 ± 0.31 |
| `v6 v0.1.0` | 49.4 ± 8.6 | 35.0 | 76.1 | 1.00 |

## js/primes.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 98.3 ± 5.6 | 89.2 | 111.6 | 1.59 ± 0.29 |
| `v6 v0.1.0` | 61.7 ± 10.6 | 47.3 | 98.0 | 1.00 |


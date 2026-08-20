# WASM/WASI benchmarks

Release build (`make BUILD_TYPE=release`), no AOT. `cli/*` benchmarks run the
`.wasm` file directly (`wasmtime run file.wasm` vs `v6 file.wasm`, WASI fully
enabled by default on both, v6 routed through its persistent daemon like any
other CLI invocation). `js/*` benchmarks load the same `.wasm` via the
`WebAssembly` API from a JS driver (`node file.js` vs `v6 file.js`, same
daemon-accelerated invocation as `bench/report.md`). hyperfine, 6 warmups,
5+ min runs per comparison, run with nothing else competing for CPU.

## cli/fib.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 51.2 ± 2.8 | 48.4 | 60.6 | 1.05 ± 0.17 |
| `v6 v0.1.0` | 49.0 ± 7.7 | 35.1 | 73.1 | 1.00 |

## cli/primes.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 31.2 ± 2.1 | 28.0 | 39.5 | 1.00 |
| `v6 v0.1.0` | 71.1 ± 9.7 | 53.9 | 85.4 | 2.28 ± 0.35 |

## cli/wasi_io.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 209.8 ± 38.7 | 163.5 | 269.2 | 1.00 |
| `v6 v0.1.0` | 354.8 ± 42.2 | 323.3 | 448.4 | 1.69 ± 0.37 |

## cli/memcpy_bulk.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 44.2 ± 2.5 | 40.6 | 51.7 | 1.00 |
| `v6 v0.1.0` | 81.8 ± 14.8 | 60.2 | 131.8 | 1.85 ± 0.35 |

## cli/table_dispatch.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 41.2 ± 10.5 | 28.3 | 68.6 | 1.00 |
| `v6 v0.1.0` | 78.8 ± 13.0 | 57.9 | 102.3 | 1.91 ± 0.58 |

## cli/dotproduct.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 47.1 ± 12.1 | 40.7 | 140.0 | 1.00 |
| `v6 v0.1.0` | 110.2 ± 12.3 | 86.1 | 143.1 | 2.34 ± 0.66 |

## js/fib.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 83.7 ± 3.7 | 78.2 | 93.5 | 1.72 ± 0.34 |
| `v6 v0.1.0` | 48.6 ± 9.5 | 35.4 | 95.9 | 1.00 |

## js/primes.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 115.7 ± 20.7 | 92.8 | 169.6 | 1.96 ± 0.47 |
| `v6 v0.1.0` | 59.0 ± 9.5 | 43.3 | 87.5 | 1.00 |

## js/memcpy_bulk.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 111.8 ± 29.5 | 83.1 | 222.4 | 1.49 ± 0.46 |
| `v6 v0.1.0` | 75.1 ± 12.1 | 58.1 | 108.5 | 1.00 |

## js/table_dispatch.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 154.7 ± 16.1 | 116.2 | 173.9 | 2.05 ± 0.39 |
| `v6 v0.1.0` | 75.6 ± 12.2 | 56.1 | 99.9 | 1.00 |

## js/dotproduct.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 100.3 ± 4.8 | 92.8 | 111.9 | 1.23 ± 0.17 |
| `v6 v0.1.0` | 81.7 ± 10.9 | 66.3 | 101.2 | 1.00 |


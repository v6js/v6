# WASM/WASI benchmarks

Release build (`make BUILD_TYPE=release`), no AOT. `cli/*` benchmarks run
the `.wasm` file directly (`wasmtime run file.wasm` vs `v6 file.wasm`, WASI
fully enabled by default on both, v6 routed through its persistent daemon
like any other CLI invocation). `js/*` benchmarks load the same `.wasm` via
the `WebAssembly` API from a JS driver (`node file.js` vs `v6 file.js`, same
daemon-accelerated invocation as `bench/report.md`). hyperfine, 6 warmups,
5+ min runs per comparison, run with nothing else competing for CPU.

## cli/fib.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 51.9 ± 3.9 | 49.4 | 69.6 | 1.07 ± 0.21 |
| `v6 v0.1.0` | 48.7 ± 8.7 | 35.0 | 64.5 | 1.00 |

## cli/primes.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 31.5 ± 1.9 | 28.4 | 38.7 | 1.00 |
| `v6 v0.1.0` | 67.8 ± 8.8 | 52.6 | 84.2 | 2.15 ± 0.31 |

## cli/wasi_io.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 217.5 ± 28.3 | 162.0 | 265.8 | 1.00 |
| `v6 v0.1.0` | 333.2 ± 12.1 | 320.6 | 351.5 | 1.53 ± 0.21 |

## cli/memcpy_bulk.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 44.3 ± 4.9 | 41.6 | 80.1 | 1.00 |
| `v6 v0.1.0` | 87.3 ± 10.8 | 64.0 | 105.0 | 1.97 ± 0.33 |

## cli/table_dispatch.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 42.8 ± 9.6 | 29.0 | 57.5 | 1.00 |
| `v6 v0.1.0` | 80.1 ± 10.8 | 60.5 | 99.0 | 1.87 ± 0.49 |

## cli/dotproduct.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 44.5 ± 1.8 | 42.2 | 52.2 | 1.00 |
| `v6 v0.1.0` | 109.7 ± 10.0 | 92.3 | 127.2 | 2.46 ± 0.25 |

## cli/dotproduct_simd.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 27.5 ± 1.0 | 24.6 | 31.0 | 1.00 |
| `v6 v0.1.0` | 291.7 ± 26.8 | 268.6 | 363.4 | 10.62 ± 1.05 |

## js/fib.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 86.3 ± 5.4 | 78.7 | 103.1 | 1.83 ± 0.32 |
| `v6 v0.1.0` | 47.2 ± 7.8 | 34.8 | 62.6 | 1.00 |

## js/primes.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 96.2 ± 3.9 | 88.7 | 106.5 | 1.53 ± 0.29 |
| `v6 v0.1.0` | 62.9 ± 11.5 | 41.3 | 80.2 | 1.00 |

## js/memcpy_bulk.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 87.1 ± 3.3 | 81.5 | 96.3 | 1.12 ± 0.15 |
| `v6 v0.1.0` | 77.9 ± 10.3 | 59.1 | 93.6 | 1.00 |

## js/table_dispatch.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 147.5 ± 16.2 | 112.7 | 172.4 | 2.03 ± 0.35 |
| `v6 v0.1.0` | 72.9 ± 9.8 | 58.0 | 94.4 | 1.00 |

## js/dotproduct.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 100.8 ± 5.2 | 93.3 | 119.5 | 1.00 |
| `v6 v0.1.0` | 113.8 ± 80.9 | 76.2 | 328.7 | 1.13 ± 0.80 |

## js/dotproduct_simd.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 77.9 ± 3.3 | 71.6 | 85.9 | 1.00 |
| `v6 v0.1.0` | 267.5 ± 11.1 | 248.0 | 281.4 | 3.44 ± 0.20 |


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
| `wasmtime v47.0.3` | 50.6 ± 1.9 | 47.4 | 60.4 | 1.05 ± 0.19 |
| `v6 v0.1.0` | 48.0 ± 8.2 | 35.2 | 84.5 | 1.00 |

## cli/primes.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 31.3 ± 1.6 | 28.3 | 36.2 | 1.00 |
| `v6 v0.1.0` | 76.5 ± 8.1 | 57.9 | 88.1 | 2.45 ± 0.29 |

## cli/wasi_io.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 204.8 ± 30.8 | 161.5 | 248.0 | 1.00 |
| `v6 v0.1.0` | 338.7 ± 8.9 | 325.0 | 350.2 | 1.65 ± 0.25 |

## cli/memcpy_bulk.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 43.0 ± 1.5 | 40.5 | 50.4 | 1.00 |
| `v6 v0.1.0` | 87.2 ± 11.6 | 64.6 | 105.2 | 2.03 ± 0.28 |

## cli/table_dispatch.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 42.9 ± 9.4 | 28.3 | 57.9 | 1.00 |
| `v6 v0.1.0` | 88.0 ± 10.7 | 68.6 | 104.2 | 2.05 ± 0.51 |

## cli/dotproduct.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 44.4 ± 1.2 | 41.2 | 50.5 | 1.00 |
| `v6 v0.1.0` | 133.0 ± 10.1 | 116.1 | 147.3 | 2.99 ± 0.24 |

## cli/dotproduct_simd.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 27.2 ± 1.1 | 24.6 | 30.2 | 1.00 |
| `v6 v0.1.0` | 308.9 ± 25.3 | 271.7 | 344.7 | 11.36 ± 1.03 |

## cli/quicksort.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 71.7 ± 1.6 | 69.8 | 78.5 | 1.00 |
| `v6 v0.1.0` | 156.9 ± 19.2 | 130.5 | 198.5 | 2.19 ± 0.27 |

## js/fib.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 85.5 ± 4.7 | 80.6 | 103.5 | 1.76 ± 0.29 |
| `v6 v0.1.0` | 48.5 ± 7.5 | 35.3 | 64.1 | 1.00 |

## js/primes.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 95.0 ± 4.5 | 88.5 | 108.5 | 1.62 ± 0.26 |
| `v6 v0.1.0` | 58.5 ± 8.8 | 43.1 | 76.8 | 1.00 |

## js/memcpy_bulk.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 87.9 ± 3.6 | 83.2 | 100.7 | 1.12 ± 0.16 |
| `v6 v0.1.0` | 78.5 ± 11.0 | 60.7 | 98.5 | 1.00 |

## js/table_dispatch.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 150.6 ± 16.7 | 119.6 | 175.0 | 1.86 ± 0.33 |
| `v6 v0.1.0` | 81.0 ± 11.4 | 60.5 | 100.1 | 1.00 |

## js/dotproduct.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 101.8 ± 4.4 | 94.3 | 113.2 | 1.00 |
| `v6 v0.1.0` | 107.6 ± 51.4 | 85.5 | 382.5 | 1.06 ± 0.51 |

## js/dotproduct_simd.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 77.6 ± 3.1 | 71.3 | 85.4 | 1.00 |
| `v6 v0.1.0` | 296.9 ± 19.2 | 272.2 | 336.8 | 3.83 ± 0.29 |

## js/quicksort.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 120.8 ± 4.6 | 113.6 | 133.0 | 1.00 |
| `v6 v0.1.0` | 153.4 ± 15.6 | 132.0 | 187.4 | 1.27 ± 0.14 |


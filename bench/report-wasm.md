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
| `wasmtime v47.0.3` | 51.7 ± 4.6 | 48.7 | 76.5 | 1.07 ± 0.17 |
| `v6 v0.1.0` | 48.4 ± 6.4 | 34.9 | 64.8 | 1.00 |

## cli/primes.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 31.6 ± 1.9 | 28.4 | 39.7 | 1.00 |
| `v6 v0.1.0` | 68.3 ± 9.6 | 52.8 | 83.7 | 2.16 ± 0.33 |

## cli/wasi_io.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 214.9 ± 21.8 | 164.8 | 229.5 | 1.00 |
| `v6 v0.1.0` | 337.4 ± 13.0 | 323.7 | 356.2 | 1.57 ± 0.17 |

## cli/memcpy_bulk.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 43.6 ± 4.6 | 40.3 | 78.9 | 1.00 |
| `v6 v0.1.0` | 77.8 ± 11.1 | 61.0 | 98.9 | 1.79 ± 0.32 |

## cli/table_dispatch.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 45.7 ± 10.1 | 28.4 | 62.2 | 1.00 |
| `v6 v0.1.0` | 77.7 ± 9.1 | 59.4 | 99.3 | 1.70 ± 0.42 |

## cli/dotproduct.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 43.7 ± 1.0 | 41.3 | 46.1 | 1.00 |
| `v6 v0.1.0` | 104.7 ± 9.2 | 88.7 | 121.7 | 2.40 ± 0.22 |

## cli/quicksort.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 71.1 ± 2.4 | 69.1 | 81.3 | 1.00 |
| `v6 v0.1.0` | 132.2 ± 10.1 | 114.4 | 150.1 | 1.86 ± 0.16 |

## js/fib.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 82.1 ± 2.6 | 77.0 | 89.9 | 1.74 ± 0.20 |
| `v6 v0.1.0` | 47.2 ± 5.1 | 34.9 | 62.2 | 1.00 |

## js/primes.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 94.8 ± 4.4 | 87.9 | 107.4 | 1.61 ± 0.23 |
| `v6 v0.1.0` | 58.9 ± 8.0 | 41.7 | 71.3 | 1.00 |

## js/memcpy_bulk.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 87.3 ± 4.9 | 80.6 | 99.9 | 1.13 ± 0.15 |
| `v6 v0.1.0` | 77.3 ± 9.6 | 61.8 | 93.4 | 1.00 |

## js/table_dispatch.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 142.9 ± 14.6 | 120.6 | 169.2 | 1.86 ± 0.31 |
| `v6 v0.1.0` | 76.9 ± 10.1 | 58.5 | 96.5 | 1.00 |

## js/dotproduct.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 101.1 ± 4.0 | 95.1 | 113.8 | 1.22 ± 0.16 |
| `v6 v0.1.0` | 82.6 ± 10.0 | 68.5 | 103.0 | 1.00 |

## js/quicksort.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 120.7 ± 4.7 | 112.9 | 131.2 | 1.00 |
| `v6 v0.1.0` | 133.5 ± 17.1 | 113.1 | 195.1 | 1.11 ± 0.15 |


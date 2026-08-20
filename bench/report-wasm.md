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
| `wasmtime v47.0.3` | 53.5 ± 7.4 | 48.6 | 84.2 | 1.13 ± 0.25 |
| `v6 v0.2.0` | 47.4 ± 8.4 | 33.9 | 61.9 | 1.00 |

## cli/primes.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 31.8 ± 2.1 | 28.0 | 38.7 | 1.00 |
| `v6 v0.2.0` | 73.0 ± 13.4 | 54.5 | 115.9 | 2.29 ± 0.45 |

## cli/wasi_io.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 231.8 ± 26.6 | 167.5 | 288.0 | 1.00 |
| `v6 v0.2.0` | 351.5 ± 28.8 | 323.9 | 415.1 | 1.52 ± 0.21 |

## cli/memcpy_bulk.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 44.7 ± 2.4 | 40.3 | 53.7 | 1.00 |
| `v6 v0.2.0` | 87.5 ± 11.4 | 65.3 | 111.4 | 1.96 ± 0.28 |

## cli/table_dispatch.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 46.4 ± 11.0 | 30.3 | 85.3 | 1.00 |
| `v6 v0.2.0` | 83.8 ± 11.7 | 68.1 | 113.3 | 1.80 ± 0.49 |

## cli/dotproduct.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 45.6 ± 1.9 | 42.4 | 51.3 | 1.00 |
| `v6 v0.2.0` | 101.5 ± 10.9 | 86.8 | 128.3 | 2.22 ± 0.26 |

## cli/quicksort.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 72.9 ± 6.6 | 68.9 | 108.7 | 1.00 |
| `v6 v0.2.0` | 132.0 ± 9.5 | 114.7 | 151.9 | 1.81 ± 0.21 |

## js/fib.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 88.1 ± 9.9 | 77.1 | 125.4 | 1.76 ± 0.72 |
| `v6 v0.2.0` | 50.0 ± 19.6 | 35.4 | 174.8 | 1.00 |

## js/primes.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 96.2 ± 6.0 | 88.7 | 122.5 | 1.54 ± 0.30 |
| `v6 v0.2.0` | 62.6 ± 11.5 | 43.6 | 89.3 | 1.00 |

## js/memcpy_bulk.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 92.5 ± 7.3 | 83.0 | 112.2 | 1.18 ± 0.19 |
| `v6 v0.2.0` | 78.5 ± 11.2 | 59.8 | 99.9 | 1.00 |

## js/table_dispatch.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 165.6 ± 21.3 | 135.4 | 223.1 | 2.03 ± 0.65 |
| `v6 v0.2.0` | 81.4 ± 24.0 | 60.0 | 187.7 | 1.00 |

## js/dotproduct.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 105.7 ± 10.8 | 96.9 | 149.3 | 1.26 ± 0.19 |
| `v6 v0.2.0` | 84.0 ± 9.8 | 67.6 | 100.6 | 1.00 |

## js/quicksort.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 121.5 ± 4.1 | 114.8 | 132.1 | 1.00 |
| `v6 v0.2.0` | 134.8 ± 12.7 | 111.9 | 161.8 | 1.11 ± 0.11 |


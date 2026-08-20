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
| `wasmtime v47.0.3` | 50.9 ± 4.6 | 47.6 | 80.1 | 1.06 ± 0.19 |
| `v6 v0.1.0` | 48.1 ± 7.4 | 35.1 | 71.2 | 1.00 |

## cli/primes.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 32.1 ± 3.0 | 27.6 | 44.1 | 1.00 |
| `v6 v0.1.0` | 66.9 ± 11.0 | 51.6 | 115.7 | 2.09 ± 0.39 |

## cli/wasi_io.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 217.4 ± 29.8 | 159.1 | 265.1 | 1.00 |
| `v6 v0.1.0` | 339.4 ± 39.8 | 318.5 | 443.1 | 1.56 ± 0.28 |

## cli/memcpy_bulk.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 42.3 ± 1.5 | 39.2 | 47.6 | 1.00 |
| `v6 v0.1.0` | 80.8 ± 9.7 | 59.3 | 97.4 | 1.91 ± 0.24 |

## cli/table_dispatch.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 45.7 ± 9.9 | 28.1 | 58.0 | 1.00 |
| `v6 v0.1.0` | 82.2 ± 10.9 | 57.9 | 112.8 | 1.80 ± 0.46 |

## cli/dotproduct.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 45.2 ± 5.7 | 41.2 | 86.1 | 1.00 |
| `v6 v0.1.0` | 103.5 ± 12.3 | 87.8 | 145.7 | 2.29 ± 0.39 |

## cli/dotproduct_simd.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 27.1 ± 1.7 | 24.1 | 33.6 | 1.00 |
| `v6 v0.1.0` | 277.2 ± 22.6 | 247.1 | 319.2 | 10.22 ± 1.05 |

## js/fib.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 86.2 ± 4.7 | 78.5 | 97.2 | 1.86 ± 0.27 |
| `v6 v0.1.0` | 46.4 ± 6.3 | 35.3 | 61.2 | 1.00 |

## js/primes.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 101.5 ± 14.0 | 92.3 | 170.6 | 1.75 ± 0.39 |
| `v6 v0.1.0` | 58.0 ± 10.0 | 41.5 | 76.6 | 1.00 |

## js/memcpy_bulk.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 89.3 ± 4.7 | 82.4 | 102.6 | 1.17 ± 0.17 |
| `v6 v0.1.0` | 76.4 ± 10.1 | 59.7 | 96.3 | 1.00 |

## js/table_dispatch.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 149.7 ± 13.2 | 127.8 | 168.6 | 1.84 ± 0.31 |
| `v6 v0.1.0` | 81.2 ± 11.6 | 60.1 | 101.0 | 1.00 |

## js/dotproduct.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 100.5 ± 3.3 | 95.0 | 106.3 | 1.26 ± 0.17 |
| `v6 v0.1.0` | 80.0 ± 10.6 | 67.4 | 106.5 | 1.00 |

## js/dotproduct_simd.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 77.5 ± 4.7 | 71.1 | 92.6 | 1.00 |
| `v6 v0.1.0` | 286.7 ± 39.9 | 243.3 | 374.6 | 3.70 ± 0.56 |


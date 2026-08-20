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
| `wasmtime v47.0.3` | 50.4 ± 4.4 | 47.2 | 78.5 | 1.02 ± 0.17 |
| `v6 v0.1.0` | 49.2 ± 7.2 | 34.5 | 67.3 | 1.00 |

## cli/primes.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 31.2 ± 1.7 | 28.1 | 37.7 | 1.00 |
| `v6 v0.1.0` | 67.7 ± 8.7 | 51.4 | 80.9 | 2.17 ± 0.30 |

## cli/wasi_io.wasm

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `wasmtime v47.0.3` | 203.5 ± 27.1 | 160.9 | 226.5 | 1.00 |
| `v6 v0.1.0` | 324.0 ± 6.3 | 317.4 | 335.0 | 1.59 ± 0.21 |

## js/fib.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 84.4 ± 5.4 | 76.3 | 98.8 | 1.71 ± 0.59 |
| `v6 v0.1.0` | 49.3 ± 16.8 | 34.5 | 173.3 | 1.00 |

## js/primes.js

| Command | Mean [ms] | Min [ms] | Max [ms] | Relative |
|:---|---:|---:|---:|---:|
| `nodejs v26.5.0` | 95.9 ± 6.3 | 88.5 | 113.1 | 1.69 ± 0.27 |
| `v6 v0.1.0` | 56.7 ± 8.3 | 42.1 | 73.4 | 1.00 |


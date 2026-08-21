### V6 v0.2.0: running WebAssembly on the JVM

V6 [compiles JavaScript to JVM bytecode](../intro/index.html). v0.2.0 adds a second compilation target to the same pipeline: a `.wasm` module compiles to a JVM class the same way a `.js` file does, runs through the same persistent daemon, and is reachable from JavaScript through the standard `WebAssembly` API.

This release is mostly about WebAssembly, what shipped, what got measured, and what got tried and reverted when the numbers didn't hold up. It also covers a real byte-backed `Uint8Array`, and the start of tracking spec conformance against test262 instead of leaving it unmeasured.

### A `.wasm` file is a JVM class, same as a `.js` file

`v6 app.wasm` parses the module and single-pass compiles each WASM function directly to a JVM method, streaming straight from the binary's function body to bytecode with no intermediate tree, then executes it through the persistent daemon. A WASM `i32`, `i64`, `f32` and `f64` map onto the JVM's own primitive stack types directly, no boxing and no `V6Value` wrapper, since a WASM function's types are already fully known from the binary's own type section and there's no dynamic property access or shape system in the way. This is the one place in the compiler that still works the way the [intro post](../intro/index.html) originally described the whole thing; see the next section for why the JS path no longer does.

```
v6 app.wasm
```

The same module loads from JavaScript through the standard `WebAssembly` API too, so code written against a browser's WebAssembly support runs unmodified.

```js
const bytes = require("fs").readFileSync("app.wasm");
WebAssembly.instantiate(bytes).then((result) => {
  console.log(result.instance.exports.run());
});
```

WASI Preview 1 covers `fd_write`, `proc_exit`, `args_get` / `args_sizes_get`, `environ_get` / `environ_sizes_get`, `random_get` and `clock_time_get`, enough for a typical compiled-to-WASM CLI program. Argv and the host environment pass through by default; `--no-wasi-args`, `--no-wasi-env`, `--no-wasi-random` and `--no-wasi-clock` deny each individually for a sandboxed run.

Routing `.wasm` execution through the same daemon the JS path already uses was the single biggest lever for CLI performance. A cold JVM boot dominates a short-lived WASM CLI invocation the same way it dominates a short-lived script, and the daemon removes that cost the same way for both.

### The JS compiler has an AST now

The [intro post](../intro/index.html) spent its first two sections on a specific architectural claim: V6's JavaScript compiler had no AST, source was lexed and parsed once, and bytecode was emitted directly during that single pass. That is no longer true, and it's worth saying plainly rather than quietly letting the old post go stale.

The JS path now parses a whole program into a real tree first (`ast_parse_program_from`, allocated into an arena), runs a hoisting pass over that completed tree before any code generation happens (`ast_hoist_scope`), and only then walks the tree a second time to emit bytecode (`ast_codegen_stmt_list`). The arena is freed once codegen finishes.

The reason is hoisting correctness, not a change of taste. JavaScript's `var`, function, class and import bindings are visible throughout their entire enclosing scope regardless of where they're lexically written, which means the compiler has to know about a declaration before it reaches it in the source. A true single-pass, emit-while-parsing design structurally cannot do that: by the time parsing reaches a `var` declared halfway through a function, code that referenced it earlier in the same function has already been emitted. Building the tree first and running a dedicated hoisting pass over it before codegen starts is the straightforward way to get this right, and it's what V6 does now.

The tradeoff the intro post described, that a no-AST compiler can't do whole-program optimizations like dead code elimination or cross-function constant folding, no longer applies either, since the whole function or program body is sitting in memory as a tree before codegen touches it. Nothing in v0.2.0 uses that yet, but the door that was previously closed is now open.

WebAssembly compilation, described above, is the one part of the compiler that still works the way the intro post originally described the whole thing: a single pass straight from the binary's bytecode to JVM bytecode, no tree. That's a reasonable fit specifically because a `.wasm` module's binary format is already fully structured and type-annotated by the time it reaches the compiler; there's no hoisting problem to solve, since WASM has no equivalent of JavaScript's declaration-visible-before-declaration semantics.

### Benchmarks against wasmtime

Seven CLI benchmarks, `.wasm` files run directly, WASI enabled, `wasmtime run file.wasm` against `v6 file.wasm`. Six JS-API benchmarks, the same modules loaded through `WebAssembly.instantiate` from a JS driver, `node file.js` against `v6 file.js`. `hyperfine`, 6 warmups, 5+ minimum timed runs, nothing else competing for CPU, on the same Intel Core i7-4800MQ machine as the [intro post's](../intro/index.html) numbers.

| CLI benchmark | wasmtime | v6 | Relative |
|:---|---:|---:|---:|
| `fib.wasm` | 51.7ms | 48.4ms | v6 1.07x faster |
| `primes.wasm` | 31.6ms | 68.3ms | 2.16x slower |
| `wasi_io.wasm` | 214.9ms | 337.4ms | 1.57x slower |
| `memcpy_bulk.wasm` | 43.6ms | 77.8ms | 1.79x slower |
| `table_dispatch.wasm` | 45.7ms | 77.7ms | 1.70x slower |
| `dotproduct.wasm` | 43.7ms | 104.7ms | 2.40x slower |
| `quicksort.wasm` | 71.1ms | 132.2ms | 1.86x slower |

| JS-API benchmark | Node | v6 | Relative |
|:---|---:|---:|---:|
| `fib.js` | 82.1ms | 47.2ms | v6 1.74x faster |
| `primes.js` | 94.8ms | 58.9ms | v6 1.61x faster |
| `memcpy_bulk.js` | 87.3ms | 77.3ms | v6 1.13x faster |
| `table_dispatch.js` | 142.9ms | 76.9ms | v6 1.86x faster |
| `dotproduct.js` | 101.1ms | 82.6ms | v6 1.22x faster |
| `quicksort.js` | 120.7ms | 133.5ms | 1.11x slower |

Against Node's own WebAssembly implementation, V8's, `v6` wins 5 of 6. Against `wasmtime` directly, `v6` only wins the pure-recursion `fib` case and loses the rest, several by more than double.

`fib` is dominated by call overhead, which the daemon already amortizes well. Everything else in the CLI table, a sieve, a bulk memcpy, an indirect call table, a dot product, an in-place quicksort, is dominated by how fast the *body* of a hot loop runs once it's already executing, and that's where the gap is real. `wasmtime`'s Cranelift compiles every function to native code once, unconditionally, before the first instruction runs. The JVM's C1/C2 tiers compile a method to native code only after it's proven hot enough across enough invocations, and V6's daemon redefines the compiled class fresh on every single request to keep each invocation's global state isolated, which means C2 never gets the chance to warm up on a short CLI benchmark's hot loop before the process's work is already done. That's an architectural gap between "compile once, always fast" and "warm up, then fast," not something a flag fixes, and it's the honest reason the CLI numbers look the way they do.

### SIMD: implemented, measured, reverted

WebAssembly SIMD (`v128` and its instructions) was implemented against `jdk.incubator.vector`, the JVM's own incubating vector API, representing a `v128` value as a Java object wrapping a `byte[16]` and reinterpreting it as an `IntVector` or `FloatVector` view for each lane-typed operation.

It didn't hold up. Measured directly against the equivalent scalar code:

| Benchmark | Time | Relative to scalar |
|:---|---:|---:|
| `dotproduct.wasm` (scalar) | 104.7ms | 1.00 |
| `dotproduct_simd.wasm` (SIMD) | 277.2ms | 2.65x slower |

The SIMD version was slower than the scalar version it was supposed to speed up, before even comparing against `wasmtime`'s native SIMD codegen. Every single vector operation allocated a fresh `byte[16]` and a fresh wrapper object, and that allocation cost swamped whatever the four-lanes-at-once arithmetic saved. A `TypedArray.prototype.fill` implementation using the same vector API was tested against a plain `java.util.Arrays.fill` call and came out statistically identical, since `Arrays.fill` on a primitive array is already a HotSpot intrinsic that gets the same wide store for free.

Both were removed rather than shipped in a state that was correct but strictly worse than not having them. WebAssembly SIMD is not implemented in v0.2.0; a module compiled with SIMD enabled fails to load.

### A real `Uint8Array`

`Uint8Array` was previously backed by the same generic property-shape machinery as an ordinary JavaScript object, meaning every byte was individually boxed as a `V6Value` and stored as a numbered property. It's now backed by an actual `byte[]`, with numeric-index reads and writes overridden to go straight to that array instead of through the generic shape system.

`fill`, `copyWithin`, `set` and `slice` are implemented directly against the byte array using `System.arraycopy` and `Arrays.fill`, both already JIT-intrinsified for primitive arrays. Every other typed array type, `Int8Array` through `Float64Array`, is still unimplemented; see [Current limitations](../../docs/index.html#current-limitations).

### Measuring spec conformance instead of leaving it unmeasured

The [intro post](../intro/index.html) listed "test262 conformance testing is not yet part of the release process" as a limitation. It now is. The full [test262](https://github.com/tc39/test262) suite is vendored, runs against every push in CI, and the results are tracked in [`test/coverage.md`](https://github.com/v6js/v6/blob/main/test/coverage.md), broken down by category, by area, and by each test's declared feature tag.

Current coverage across the `language`, `built-ins` and `annexB` categories is 21.10%. That number is a starting point, not a target hit this release, and it's deliberately unflattering in places: `Proxy`, `Reflect` and every typed array type besides `Uint8Array` sit at 0%, which matches what's actually implemented rather than papering over it.

Building the runner surfaced a real bug independent of test262 itself: the daemon degrades under sustained request volume, a `StackOverflowError` starts appearing on every request after enough of them, reproducible with nothing more than the test harness's own setup code and unrelated to which specific test triggers it. It's being tracked, not fixed in this release; the test262 runner works around it by restarting the daemon periodically during a full run so the measurement itself stays valid.

### General JavaScript benchmarks, revisited

The 40-fixture suite from the [intro post](../intro/index.html) was rerun on this release. `v6`'s persistent process is now faster than Node on 25 of 40, down from 30 of 40 at the time of that post. Nothing in this release touched the JS execution path directly; the shift reflects normal run-to-run variance on the same fixtures plus the cumulative effect of everything else that changed underneath it, and is called out here rather than left for someone else to notice the number moved.

### Current limitations

`Proxy`, `Reflect`, `eval`, the `vm` module, the dynamic `import()` expression and typed array types other than `Uint8Array` remain unimplemented. WebAssembly SIMD is unimplemented, by decision rather than by omission, for the reasons above.

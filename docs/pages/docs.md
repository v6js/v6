### Documentation

V6 is a JavaScript runtime built on the JVM. It compiles JavaScript directly to JVM bytecode, runs it with a persistent warm process so repeated invocations start fast, and can call into any Java library already on the classpath through a built-in `java:` import scheme.

V6 is experimental. Test262 conformance testing is not yet part of the release process, so treat spec conformance beyond what the fixture and compatibility test suites happen to exercise as unverified.

### Installation

Each release ships two variants per platform.

#### Developer build

A small download that uses a JDK already installed on your machine through `JAVA_HOME`. Pick this if you already have a JDK on the machine.

#### Portable build

A self-contained download that bundles a full JDK. Pick this if you want to run V6 on a machine without installing a JDK first.

#### Downloads

| Platform | Developer | Portable |
|---|---|---|
| Linux x86_64 | [v6-x86_64-linux-gnu-developer](https://github.com/v6js/v6/releases/latest/download/v6-x86_64-linux-gnu-developer.zip) | [v6-x86_64-linux-gnu-portable](https://github.com/v6js/v6/releases/latest/download/v6-x86_64-linux-gnu-portable.zip) |
| Linux arm64 | [v6-aarch64-linux-gnu-developer](https://github.com/v6js/v6/releases/latest/download/v6-aarch64-linux-gnu-developer.zip) | [v6-aarch64-linux-gnu-portable](https://github.com/v6js/v6/releases/latest/download/v6-aarch64-linux-gnu-portable.zip) |
| Windows x86_64 | [v6-x86_64-windows-gnu-developer](https://github.com/v6js/v6/releases/latest/download/v6-x86_64-windows-gnu-developer.zip) | [v6-x86_64-windows-gnu-portable](https://github.com/v6js/v6/releases/latest/download/v6-x86_64-windows-gnu-portable.zip) |
| macOS x86_64 | [v6-x86_64-macos-developer](https://github.com/v6js/v6/releases/latest/download/v6-x86_64-macos-developer.zip) | [v6-x86_64-macos-portable](https://github.com/v6js/v6/releases/latest/download/v6-x86_64-macos-portable.zip) |
| macOS arm64 | [v6-aarch64-macos-developer](https://github.com/v6js/v6/releases/latest/download/v6-aarch64-macos-developer.zip) | [v6-aarch64-macos-portable](https://github.com/v6js/v6/releases/latest/download/v6-aarch64-macos-portable.zip) |

Unzip the archive and add the folder to your `PATH`.

### Getting started

Run a script.

```
v6 app.js
```

Evaluate an inline expression.

```
v6 -e "console.log(1 + 2)"
```

Start the REPL.

```
v6
```

Press Ctrl+C once inside the REPL to abort the expression being typed and again to exit. Ctrl+D exits directly.

### CLI reference

#### Flags

- `-e, --eval <code>` evaluate code and exit
- `-o <output.jar>` compile to a standalone JAR instead of running through the persistent process
- `-cp, --classpath <path>` extra Java classpath entries for `java:` imports
- `--no-daemon` skip the persistent warm process for this invocation
- `--color` force colored output
- `--no-color` disable colored output
- `-v, --version` print the version and exit
- `-h, --help` print help and exit

#### REPL commands

- `.help` show REPL commands
- `.exit` exit the REPL
- `.break` cancel the current multi-line input
- `.clear` reset the session
- `.history` print input history
- `.save <file>` save session source to a file
- `.load <file>` load and run a file in the session

### The persistent process

Running `v6 app.js` compiles the script and its dependency graph, then hands the resulting bytecode to a background process instead of booting a fresh JVM for every invocation.

The first invocation on a machine starts that background process. Every subsequent invocation reuses it and pays no JVM startup cost, which is why repeated runs of the same or a different script feel close to instant after the first one.

Pass `--no-daemon` to skip this and run in a fresh, standalone JVM for that one invocation instead. This is mostly useful for isolating a run from any state a long-lived background process might be holding, or when debugging the process boundary itself.

### AOT compilation

Passing `-o app.jar` skips the persistent process entirely and packages the compiled classes, along with V6's own runtime classes, into a single runnable JAR.

```
v6 app.js -o app.jar
java -jar app.jar
```

That JAR runs with a plain `java -jar app.jar` and has no dependency on V6 being installed on the machine that runs it. The tradeoff is startup cost. Every invocation of the JAR pays a full JVM boot, since there is no persistent process to reuse.

Reach for this when a standalone artifact matters more than repeated-invocation latency, such as shipping a CLI tool to a machine that only has a JDK on it and nothing else.

### Compile caching

Compilation is not free, and for a multi-file program pulling in real dependencies, most of the wall-clock cost of a fresh invocation is spent recompiling files that have not changed since the last run.

V6 caches compiled output on disk, keyed by the entry script's absolute path together with the modification time and size of every file that was touched while compiling it, including every module reached transitively through `require` or `import`.

On the next invocation, if the entry script and every one of those tracked files still match the recorded modification time and size, compilation is skipped and the cached bytecode runs directly. Editing any single file in the dependency graph, even one several `require` calls deep, invalidates the cache for that entry point and forces a fresh compile on the next run.

This is the same trust model `make` and `ccache` use for deciding whether a build step needs to rerun, applied to a JavaScript module graph instead of to object files.

### Architecture

V6 has no intermediate representation. Source text is lexed and parsed once, and bytecode is emitted directly during that single pass, rather than building an AST first and generating code from it afterward.

#### From source to class file

Every module compiles to its own JVM class. The entry script compiles to a class named `Main` with a `public static void main(String[])` method. A CommonJS module pulled in through `require` compiles to a class named `ModN`, where `N` is the order it was first encountered in, exposing a `moduleExports()` method that populates and caches `module.exports` on first call. An ES module compiles the same way but exposes an `exports()` method, with export bindings resolved statically by scanning the module body for `export` syntax.

#### Values and property access

Every JavaScript value at runtime is a `V6Value`, an immutable object carrying a tag, a `double` and an `Object` reference. The tag distinguishes number, boolean, null, undefined, object, string, function and bigint. A number that fits in a small preallocated range is served from a cache array instead of allocating a new value, since integer literals and loop counters commonly fall in that range.

Property access on plain objects goes through a shape system. Each object starts on a shared empty shape, and adding a property transitions it to a new shape that remembers the slot index for that key, so objects that add the same keys in the same order share a shape and turn property reads into array indexing instead of a hash lookup, for the first 24 properties added to an object. An object that grows past that falls back to a dictionary.

#### JVM bytecode limits

Targeting the JVM means designing around constraints a native-code compiler never has to think about. A method body is capped at 65535 bytes of bytecode, the constant pool is capped at 65535 entries and local variable slots are addressed with an unsigned 16-bit index.

A JavaScript function whose compiled body would exceed the method size limit, generated code being the realistic case rather than hand-written code, is split automatically across a chain of helper methods at compile time, with its local variables promoted to a shared frame so the split is invisible from JavaScript. Embedding a `.json` module as a compiled string constant is chunked into pieces under the constant pool's string encoding limit and concatenated back together at class-init time, for the same reason.

### Modules

`require` (CommonJS) and `import` (ES modules) both work, including within the same project and the same file tree.

#### CommonJS or ES module

Whether a file is treated as CommonJS or an ES module is decided by scanning the file itself for `export` syntax, rather than trusting a `.mjs` or `.cjs` extension or a `package.json` `type` field. This keeps the detection correct even when a package's own metadata disagrees with what the file actually contains.

#### Resolving a specifier

A relative specifier such as `./util` or `../lib/thing` resolves against the directory of the file doing the importing, trying the path as a file first (adding `.js` if needed) and then as a directory (reading its `package.json` `main` or `exports` field, falling back to `index.js`).

A bare specifier such as `react` or `lodash/fp` walks up from the importing file's directory looking for a `node_modules` folder containing that package at each level, the same algorithm Node uses, stopping at the first match found while walking toward the filesystem root.

Every resolved path is canonicalized before it is used as a cache key, so a package reached through a symlink, such as a locally linked workspace package, resolves to the same module instance as the same package reached by a direct path. Two different paths that point at the same file on disk are never treated as two different modules.

#### Circular requires and circular imports

Both work. A CommonJS module that requires something which, transitively, requires it back receives whatever partial `module.exports` had been assigned by the time the cycle closed, matching Node's own behavior. An ES module cycle resolves through live bindings the same way.

### Java interop

Java code is reachable from JavaScript through a `java:` import scheme.

```js
import ArrayList from "java:java.util.ArrayList";

const list = new ArrayList();
list.add("hello");
console.log(list.get(0));
```

Resolving `java:java.util.ArrayList` calls `Class.forName` against that fully qualified name at runtime. The result wraps the `Class` object and exposes `new`, static methods and static fields as ordinary JavaScript operations. No binding generation step and no separate interface definition file are involved.

#### Overload resolution

Calling a Java method or constructor from JavaScript scores every candidate method or constructor with that name against the arguments actually passed, matching each JavaScript argument against each candidate's parameter types including varargs expansion, and picks the closest match. This is the same problem Java's own compiler solves at compile time, resolved here at call time through reflection instead.

#### Implementing a Java interface from JavaScript

Passing a plain JavaScript function or object anywhere a Java interface type is expected creates a `java.lang.reflect.Proxy` that forwards each interface method call back into the corresponding JavaScript function or property, marshalling arguments and the return value between V6's internal value representation and native Java types.

```js
import { JFrame, JButton, JTextField } from "java:javax.swing";
import BorderLayout from "java:java.awt.BorderLayout";

const frame = new JFrame("v6 demo");
const input = new JTextField(20);
const button = new JButton("Add");

button.addActionListener((e) => {
  console.log("clicked with input:", input.getText());
});

frame.getContentPane().add(input, BorderLayout.CENTER);
frame.getContentPane().add(button, BorderLayout.SOUTH);
frame.setSize(300, 100);
frame.setVisible(true);
```

`addActionListener` expects a Java `ActionListener`. Passing an arrow function directly is enough, and this works against any Java library already on the classpath, not only ones V6 ships bindings for.

#### Database access

Any JDBC driver on the classpath is reachable the same way, with no ORM layer or query builder standing between JavaScript and the driver.

```js
import { DriverManager } from "java:java.sql";

const conn = DriverManager.getConnection("jdbc:sqlite:app.db");
const stmt = conn.createStatement();
const rs = stmt.executeQuery("SELECT id, name FROM users");

while (rs.next()) {
  console.log(rs.getInt("id"), rs.getString("name"));
}

conn.close();
```

Add the driver's JAR to the classpath with `-cp` if it is not already on it.

#### Calls from a foreign thread

A Java callback into JavaScript does not always originate on V6's own thread. A Swing button click fires on the AWT event dispatch thread, a `java.util.Timer` fires on its own timer thread, and neither of those threads is one V6 spawned itself.

When that happens, the call is posted onto V6's event loop and the calling Java thread blocks on a latch until the JavaScript callback finishes running there, keeping the actual JavaScript execution single-threaded regardless of which Java thread triggered it. The one exception is an interface method whose return type is `void`, where the callback is posted and the calling Java thread does not wait for it to run.

### Error handling

`try`, `catch`, `finally` and `throw` all work as the spec describes. An error left uncaught propagates out of the script, gets formatted and printed to stderr, and the process exits with a non-zero status.

`Error` and its standard subclasses, `TypeError`, `RangeError`, `SyntaxError`, `ReferenceError`, `EvalError` and `URIError`, are all available globally and behave like their Node counterparts, including `.message`, `.name` and `.stack`.

### Compatibility

V6 is tested against real npm packages, not only synthetic fixtures, including `express`, `fast-glob`, `react` and `react-dom`, with output diffed against the same code running under real Node.

#### Global APIs

- [x] `console`
- [x] `globalThis` / `global`
- [x] `Math`
- [x] `JSON`
- [x] `Object`
- [x] `Array`
- [x] `String`
- [x] `Number`
- [x] `Boolean`
- [x] `Date`
- [x] `RegExp`
- [x] `Function`
- [x] `Symbol`
- [x] `Map` / `Set`
- [x] `WeakMap` / `WeakSet`
- [x] `Promise`
- [x] `Error` and its subclasses
- [x] `BigInt`
- [x] `Buffer`
- [x] `Uint8Array`
- [x] `URL` / `URLSearchParams`
- [x] `setTimeout` / `setInterval` / `setImmediate` / `queueMicrotask`
- [x] `atob` / `btoa`
- [x] `encodeURIComponent` / `decodeURIComponent` / `encodeURI` / `decodeURI`
- [ ] `eval`
- [ ] `Proxy`
- [ ] `Reflect`
- [ ] `WeakRef` / `FinalizationRegistry`
- [ ] `AggregateError`
- [ ] other typed array types

#### Isomorphic Web APIs

- [x] `fetch`
- [x] `Headers` / `Request` / `Response`
- [x] `Blob` / `File` / `FormData`
- [x] `ReadableStream` / `WritableStream` / `TransformStream`
- [x] `TextEncoder` / `TextDecoder`
- [x] `TextEncoderStream` / `TextDecoderStream`
- [x] `CompressionStream` / `DecompressionStream`
- [x] `ArrayBuffer`
- [x] `crypto` / `CryptoKey`
- [x] `structuredClone`
- [x] `Event` / `CustomEvent` / `EventTarget`
- [x] `AbortController` / `AbortSignal`
- [x] `WebSocket`
- [x] `EventSource`
- [x] `MessageChannel` / `MessagePort` / `MessageEvent`
- [x] `BroadcastChannel`
- [x] `Worker` / `self`
- [x] `performance`
- [x] `navigator`
- [ ] `WebAssembly`

#### Node.js compatibility

- [x] `path`
- [x] `buffer`
- [x] `util`
- [x] `os`
- [x] `tty`
- [x] `fs`
- [x] `events`
- [x] `assert`
- [x] `querystring`
- [x] `perf_hooks`
- [x] `dns`
- [x] `string_decoder`
- [x] `url`
- [x] `zlib`
- [x] `crypto`
- [x] `stream`
- [x] `child_process`
- [x] `net`
- [x] `http` / `https`
- [x] `tls`
- [x] `readline`
- [x] `worker_threads`
- [x] `cluster`
- [x] `repl`
- [x] `timers`
- [x] `dgram`
- [x] `http2`
- [x] `v8`
- [x] `module`
- [x] `diagnostics_channel`
- [x] `async_hooks`
- [x] `inspector`
- [x] `trace_events`
- [ ] `vm`

### Benchmarks

The `bench/` suite in the V6 repository runs 40 fixtures, each covering a different language or standard library feature, under `node` and under `v6` in its default persistent-process mode.

V6's persistent process is faster than Node on 30 of these 40 fixtures. A standalone AOT JAR is slower than Node on all 40, since each run pays a full `java -jar` startup cost instead of reusing a warm process, so that mode is left out of the comparison entirely rather than mixed into a same-scale benchmark against two already-warm processes.

Measured with `hyperfine`, using 6 warmup runs followed by a minimum of 5 timed runs per command, on an Intel Core i7-4800MQ (Haswell, 4 cores, 8 threads) with 8 GB of RAM, against `node v26.5.0` and `v6 v0.1.0`.

See the [introductory blog post](../blog/intro/index.html) for the full breakdown by category, with charts.

### FAQ

#### Why does the first run of a script feel slower than the rest

The first invocation on a machine starts V6's persistent background process and compiles the script from scratch. Every invocation after that reuses the already-running process and the on-disk compile cache, so it only recompiles files that changed since the last run. See [The persistent process](#the-persistent-process) and [Compile caching](#compile-caching).

#### Can V6 run TypeScript directly

No. V6 runs JavaScript. Compile TypeScript to JavaScript first, with `tsc` or a bundler, and run the compiled output with V6.

#### Is V6 ready for production use

V6 is experimental. Treat it as suitable for tools, scripts, prototypes and embedding scenarios where the tradeoffs make sense today, not as a drop-in replacement for Node in a production service.

#### Where do I report a bug or ask a question

Open an issue on [GitHub](https://github.com/v6js/v6).

### Current limitations

`WebAssembly`, `Proxy`, `Reflect`, `eval` and the `vm` module are not implemented.

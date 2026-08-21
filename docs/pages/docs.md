### Documentation

V6 is a JavaScript runtime built on the JVM. It compiles JavaScript directly to JVM bytecode, runs it with a persistent warm process so repeated invocations start fast, and can call into any Java library already on the classpath through a built-in `java:` import scheme.

V6 is experimental. The full [test262](https://github.com/tc39/test262) conformance suite runs against V6 on every push, with results tracked in [test/coverage.md](https://github.com/v6js/v6/blob/main/test/coverage.md). Current coverage across the `language`, `built-ins` and `annexB` categories is 21.10%, so treat spec conformance beyond that measured baseline as unverified.

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
- `--no-wasi-args` don't pass CLI args through as WASI argv, when running a `.wasm` file
- `--no-wasi-env` don't pass host environment variables through to WASI
- `--no-wasi-random` deny the WASI `random_get` syscall
- `--no-wasi-clock` deny the WASI `clock_time_get` syscall

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

### WebAssembly

A `.wasm` file runs directly from the CLI, compiled to a JVM class the same way a `.js` entry script is and executed through the same persistent process.

```
v6 app.wasm
```

WASI Preview 1 is supported for the syscalls a typical compiled-to-WASM program needs: `fd_write`, `proc_exit`, `args_get` / `args_sizes_get`, `environ_get` / `environ_sizes_get`, `random_get` and `clock_time_get`. Argv and the host environment are passed through by default; `--no-wasi-args`, `--no-wasi-env`, `--no-wasi-random` and `--no-wasi-clock` deny each of those individually for a sandboxed run.

The same `.wasm` module also loads from JavaScript through the standard `WebAssembly` API, so code written against a browser's WebAssembly support runs unmodified.

```js
const fs = require("fs");
const bytes = fs.readFileSync("app.wasm");

WebAssembly.instantiate(bytes).then((result) => {
  console.log(result.instance.exports.run());
});
```

WebAssembly SIMD (the `v128` type and its instructions) is not implemented. A module compiled with SIMD enabled fails to load. See [Current limitations](#current-limitations).

### Compile caching

Compilation is not free, and for a multi-file program pulling in real dependencies, most of the wall-clock cost of a fresh invocation is spent recompiling files that have not changed since the last run.

V6 caches compiled output on disk, keyed by the entry script's absolute path together with the modification time and size of every file that was touched while compiling it, including every module reached transitively through `require` or `import`.

On the next invocation, if the entry script and every one of those tracked files still match the recorded modification time and size, compilation is skipped and the cached bytecode runs directly. Editing any single file in the dependency graph, even one several `require` calls deep, invalidates the cache for that entry point and forces a fresh compile on the next run.

This is the same trust model `make` and `ccache` use for deciding whether a build step needs to rerun, applied to a JavaScript module graph instead of to object files.

### Architecture

JavaScript source is parsed into a real AST first, then compiled to bytecode in two further passes over that tree: a hoisting pass that declares every `var`, function, class and import binding in a scope before any of that scope's code is generated, followed by the codegen pass itself. This exists because JavaScript's hoisting semantics make a bindings-visible-before-declaration guarantee that a true single-pass, emit-while-parsing compiler cannot satisfy on its own. WebAssembly compilation is the exception: a `.wasm` module's binary format is already fully typed and structured, so it compiles in a genuine single pass straight from the binary to JVM bytecode, no tree involved. See the [v0.2.0 release notes](../blog/v0-2-0/index.html) for the reasoning behind the JS side of this.

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

#### Other globals

`console`, `globalThis` / `global`, `setTimeout` / `setInterval` / `setImmediate` / `queueMicrotask`, `atob` / `btoa` and `encodeURIComponent` / `decodeURIComponent` / `encodeURI` / `decodeURI` are all present as plain functions and behave like their Node/browser counterparts; there is no further API surface on any of them worth breaking down.

#### Global APIs (methods)

#### Array

Static: [isArray](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/isArray), [from](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/from), [of](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/of)

Instance: [push](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/push), [pop](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/pop), [shift](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/shift), [unshift](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/unshift), [slice](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/slice), [splice](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/splice), [indexOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/indexOf), [includes](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/includes), [join](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/join), [map](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/map), [filter](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/filter), [forEach](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/forEach), [some](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/some), [every](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/every), [find](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/find), [findIndex](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/findIndex), [reduce](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/reduce), [concat](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/concat), [reverse](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/reverse), [sort](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/sort), [flat](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/flat), [flatMap](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/flatMap)

#### Object

Static: [keys](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/keys), [values](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/values), [entries](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/entries), [assign](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/assign), [freeze](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/freeze), [isFrozen](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/isFrozen), [getOwnPropertyDescriptor](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/getOwnPropertyDescriptor), [getOwnPropertyNames](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/getOwnPropertyNames), [hasOwn](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/hasOwn), [seal](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/seal), [isSealed](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/isSealed), [create](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/create), [getPrototypeOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/getPrototypeOf), [setPrototypeOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/setPrototypeOf), [fromEntries](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/fromEntries), [is](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/is), [defineProperty](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/defineProperty), [defineProperties](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/defineProperties)

Instance: [hasOwnProperty](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/hasOwnProperty), [isPrototypeOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/isPrototypeOf), [propertyIsEnumerable](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/propertyIsEnumerable), [toString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/toString), [valueOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/valueOf)

#### Math

Methods: [abs](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/abs), [floor](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/floor), [ceil](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/ceil), [round](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/round), [trunc](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/trunc), [sqrt](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/sqrt), [cbrt](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/cbrt), [sign](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/sign), [max](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/max), [min](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/min), [pow](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/pow), [random](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/random), [log](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/log), [log2](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/log2), [log10](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/log10), [exp](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/exp), [sin](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/sin), [cos](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/cos), [tan](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/tan), [asin](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/asin), [acos](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/acos), [atan](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/atan), [atan2](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/atan2), [clz32](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/clz32)

Constants: [PI](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/PI), [E](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/E), [LN2](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/LN2), [LN10](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/LN10), [LOG2E](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/LOG2E), [LOG10E](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/LOG10E), [SQRT2](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/SQRT2), [SQRT1_2](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/SQRT1_2)

#### String

Static: [fromCharCode](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/fromCharCode), [fromCodePoint](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/fromCodePoint)

Instance: [charAt](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/charAt), [charCodeAt](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/charCodeAt), [codePointAt](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/codePointAt), [at](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/at), [indexOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/indexOf), [lastIndexOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/lastIndexOf), [includes](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/includes), [startsWith](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/startsWith), [endsWith](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/endsWith), [slice](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/slice), [substring](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/substring), [toUpperCase](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/toUpperCase), [toLowerCase](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/toLowerCase), [trim](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/trim), [trimStart](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/trimStart), [trimEnd](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/trimEnd), [split](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/split), [replace](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/replace), [replaceAll](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/replaceAll), [match](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/match), [matchAll](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/matchAll), [search](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/search), [repeat](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/repeat), [padStart](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/padStart), [padEnd](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/padEnd), [concat](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/concat), [toString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/toString), [valueOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/valueOf)

#### Number

Static: [isInteger](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/isInteger), [isFinite](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/isFinite), [isNaN](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/isNaN), [isSafeInteger](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/isSafeInteger), [parseFloat](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/parseFloat), [parseInt](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/parseInt), [EPSILON](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/EPSILON), [MAX_SAFE_INTEGER](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/MAX_SAFE_INTEGER), [MIN_SAFE_INTEGER](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/MIN_SAFE_INTEGER), [MAX_VALUE](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/MAX_VALUE), [MIN_VALUE](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/MIN_VALUE), [POSITIVE_INFINITY](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/POSITIVE_INFINITY), [NEGATIVE_INFINITY](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/NEGATIVE_INFINITY), [NaN](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/NaN)

Instance: [toFixed](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/toFixed), [toPrecision](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/toPrecision), [toString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/toString), [valueOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number/valueOf)

#### Boolean

No instance methods beyond the [inherited defaults](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/toString).

#### RegExp

Instance: [test](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/test), [exec](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/exec), [toString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/toString), plus [source](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/source), [flags](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/flags), [global](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/global), [ignoreCase](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/ignoreCase), [multiline](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/multiline), [sticky](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/sticky), [unicode](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/unicode), [lastIndex](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/lastIndex)

#### Map

Instance: [get](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/get), [set](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/set), [has](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/has), [delete](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/delete), [clear](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/clear), [forEach](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/forEach), [keys](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/keys), [values](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/values), [entries](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/entries), [size](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/size)

#### Set

Instance: [add](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/add), [has](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/has), [delete](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/delete), [clear](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/clear), [forEach](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/forEach), [values](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/values), [keys](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/keys), [entries](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/entries), [size](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/size)

#### WeakMap

Instance: same surface as [Map](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Map/) above -- note this is not spec-accurate weak-reference behavior, see [Current limitations](#current-limitations).

#### WeakSet

Instance: same surface as [Set](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Set/) above -- same caveat as `WeakMap`.

#### Promise

Static: [resolve](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/resolve), [reject](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/reject), [all](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/all), [race](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/race), [allSettled](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/allSettled), [any](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/any)

Instance: [then](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/then), [catch](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/catch), [finally](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/finally)

#### Date

Static: [now](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/now), [parse](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/parse), [UTC](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/UTC)

Instance: [getTime](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getTime), [valueOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/valueOf), [setTime](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setTime), [getFullYear](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getFullYear), [getMonth](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getMonth), [getDate](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getDate), [getDay](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getDay), [getHours](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getHours), [getMinutes](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getMinutes), [getSeconds](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getSeconds), [getMilliseconds](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getMilliseconds), [getUTCFullYear](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getUTCFullYear), [getUTCMonth](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getUTCMonth), [getUTCDate](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getUTCDate), [getUTCDay](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getUTCDay), [getUTCHours](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getUTCHours), [getUTCMinutes](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getUTCMinutes), [getUTCSeconds](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getUTCSeconds), [getUTCMilliseconds](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getUTCMilliseconds), [getYear](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getYear), [getTimezoneOffset](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/getTimezoneOffset), [setFullYear](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setFullYear), [setMonth](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setMonth), [setDate](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setDate), [setHours](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setHours), [setMinutes](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setMinutes), [setSeconds](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setSeconds), [setMilliseconds](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setMilliseconds), [setUTCFullYear](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setUTCFullYear), [setUTCMonth](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setUTCMonth), [setUTCDate](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setUTCDate), [setUTCHours](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setUTCHours), [setUTCMinutes](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setUTCMinutes), [setUTCSeconds](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setUTCSeconds), [setUTCMilliseconds](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/setUTCMilliseconds), [toISOString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toISOString), [toJSON](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toJSON), [toString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toString), [toDateString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toDateString), [toTimeString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toTimeString), [toUTCString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toUTCString), [toGMTString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toGMTString), [toLocaleDateString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleDateString), [toLocaleTimeString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleTimeString), [toLocaleString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Date/toLocaleString)

#### JSON

Methods: [parse](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/parse), [stringify](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify)

#### Symbol

Callable as `Symbol(desc)`. Static: [iterator](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol/iterator), [for](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol/for)

#### Error and its subclasses

`TypeError`, `RangeError`, `SyntaxError`, `ReferenceError`, `EvalError` and `URIError` all chain to `Error.prototype`. Static: [captureStackTrace](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error/captureStackTrace), `stackTraceLimit`, `prepareStackTrace`. Instance: [toString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error/toString), `name`, `message`.

#### BigInt

Callable as `BigInt(value)`. Instance: [toString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigInt/toString), [toLocaleString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigInt/toLocaleString), [valueOf](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/BigInt/valueOf)

#### Function

Instance: [call](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function/call), [apply](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function/apply), [bind](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function/bind), [toString](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function/toString). `new Function(...)` is not supported.

#### Uint8Array

Static: [Uint8Array.from](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/from), [Uint8Array.of](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/of). Instance: [fill](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/fill), [copyWithin](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/copyWithin), [set](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/set), [slice](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/slice). No other typed array types are implemented, see [Current limitations](#current-limitations).

### Isomorphic Web APIs (methods)

#### fetch

Global function: [fetch](https://developer.mozilla.org/en-US/docs/Web/API/Window/fetch).

#### Headers

Instance: [append](https://developer.mozilla.org/en-US/docs/Web/API/Headers/append), [set](https://developer.mozilla.org/en-US/docs/Web/API/Headers/set), [get](https://developer.mozilla.org/en-US/docs/Web/API/Headers/get), [has](https://developer.mozilla.org/en-US/docs/Web/API/Headers/has), [delete](https://developer.mozilla.org/en-US/docs/Web/API/Headers/delete), [forEach](https://developer.mozilla.org/en-US/docs/Web/API/Headers/forEach), [keys](https://developer.mozilla.org/en-US/docs/Web/API/Headers/keys), [values](https://developer.mozilla.org/en-US/docs/Web/API/Headers/values), [entries](https://developer.mozilla.org/en-US/docs/Web/API/Headers/entries)

#### Request

Getters: [url](https://developer.mozilla.org/en-US/docs/Web/API/Request/url), [method](https://developer.mozilla.org/en-US/docs/Web/API/Request/method), [headers](https://developer.mozilla.org/en-US/docs/Web/API/Request/headers), [signal](https://developer.mozilla.org/en-US/docs/Web/API/Request/signal), [bodyUsed](https://developer.mozilla.org/en-US/docs/Web/API/Request/bodyUsed). Methods: [text](https://developer.mozilla.org/en-US/docs/Web/API/Request/text), [json](https://developer.mozilla.org/en-US/docs/Web/API/Request/json), [arrayBuffer](https://developer.mozilla.org/en-US/docs/Web/API/Request/arrayBuffer), [blob](https://developer.mozilla.org/en-US/docs/Web/API/Request/blob), [clone](https://developer.mozilla.org/en-US/docs/Web/API/Request/clone)

#### Response

Static: [error](https://developer.mozilla.org/en-US/docs/Web/API/Response/error), [redirect](https://developer.mozilla.org/en-US/docs/Web/API/Response/redirect), [json](https://developer.mozilla.org/en-US/docs/Web/API/Response/json). Getters: [status](https://developer.mozilla.org/en-US/docs/Web/API/Response/status), [statusText](https://developer.mozilla.org/en-US/docs/Web/API/Response/statusText), [ok](https://developer.mozilla.org/en-US/docs/Web/API/Response/ok), [headers](https://developer.mozilla.org/en-US/docs/Web/API/Response/headers), [url](https://developer.mozilla.org/en-US/docs/Web/API/Response/url), [redirected](https://developer.mozilla.org/en-US/docs/Web/API/Response/redirected), [type](https://developer.mozilla.org/en-US/docs/Web/API/Response/type), [bodyUsed](https://developer.mozilla.org/en-US/docs/Web/API/Response/bodyUsed). Methods: [text](https://developer.mozilla.org/en-US/docs/Web/API/Response/text), [json](https://developer.mozilla.org/en-US/docs/Web/API/Response/json), [arrayBuffer](https://developer.mozilla.org/en-US/docs/Web/API/Response/arrayBuffer), [bytes](https://developer.mozilla.org/en-US/docs/Web/API/Response/bytes), [blob](https://developer.mozilla.org/en-US/docs/Web/API/Response/blob), [clone](https://developer.mozilla.org/en-US/docs/Web/API/Response/clone)

#### Blob

Getters: [size](https://developer.mozilla.org/en-US/docs/Web/API/Blob/size), [type](https://developer.mozilla.org/en-US/docs/Web/API/Blob/type). Methods: [slice](https://developer.mozilla.org/en-US/docs/Web/API/Blob/slice), [arrayBuffer](https://developer.mozilla.org/en-US/docs/Web/API/Blob/arrayBuffer), [bytes](https://developer.mozilla.org/en-US/docs/Web/API/Blob/bytes), [text](https://developer.mozilla.org/en-US/docs/Web/API/Blob/text), [stream](https://developer.mozilla.org/en-US/docs/Web/API/Blob/stream)

#### File

Extends `Blob`. Getters: [name](https://developer.mozilla.org/en-US/docs/Web/API/File/name), [lastModified](https://developer.mozilla.org/en-US/docs/Web/API/File/lastModified)

#### FormData

Instance: [append](https://developer.mozilla.org/en-US/docs/Web/API/FormData/append), [set](https://developer.mozilla.org/en-US/docs/Web/API/FormData/set), [get](https://developer.mozilla.org/en-US/docs/Web/API/FormData/get), [getAll](https://developer.mozilla.org/en-US/docs/Web/API/FormData/getAll), [has](https://developer.mozilla.org/en-US/docs/Web/API/FormData/has), [delete](https://developer.mozilla.org/en-US/docs/Web/API/FormData/delete), [forEach](https://developer.mozilla.org/en-US/docs/Web/API/FormData/forEach), [keys](https://developer.mozilla.org/en-US/docs/Web/API/FormData/keys), [values](https://developer.mozilla.org/en-US/docs/Web/API/FormData/values), [entries](https://developer.mozilla.org/en-US/docs/Web/API/FormData/entries)

#### ReadableStream

Getter: [locked](https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/locked). Methods: [getReader](https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/getReader), [cancel](https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/cancel), [pipeTo](https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/pipeTo), [pipeThrough](https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/pipeThrough), [tee](https://developer.mozilla.org/en-US/docs/Web/API/ReadableStream/tee)

#### WritableStream

Getter: [locked](https://developer.mozilla.org/en-US/docs/Web/API/WritableStream/locked). Methods: [getWriter](https://developer.mozilla.org/en-US/docs/Web/API/WritableStream/getWriter), [close](https://developer.mozilla.org/en-US/docs/Web/API/WritableStream/close), [abort](https://developer.mozilla.org/en-US/docs/Web/API/WritableStream/abort), [write](https://developer.mozilla.org/en-US/docs/Web/API/WritableStream/write)

#### TransformStream

Constructs a `{readable, writable}` pair; see [TransformStream](https://developer.mozilla.org/en-US/docs/Web/API/TransformStream/TransformStream).

#### TextEncoder

Getter: [encoding](https://developer.mozilla.org/en-US/docs/Web/API/TextEncoder/encoding). Methods: [encode](https://developer.mozilla.org/en-US/docs/Web/API/TextEncoder/encode), [encodeInto](https://developer.mozilla.org/en-US/docs/Web/API/TextEncoder/encodeInto)

#### TextDecoder

Getters: [encoding](https://developer.mozilla.org/en-US/docs/Web/API/TextDecoder/encoding), [fatal](https://developer.mozilla.org/en-US/docs/Web/API/TextDecoder/fatal), [ignoreBOM](https://developer.mozilla.org/en-US/docs/Web/API/TextDecoder/ignoreBOM). Method: [decode](https://developer.mozilla.org/en-US/docs/Web/API/TextDecoder/decode)

#### TextEncoderStream / TextDecoderStream

Built as `TransformStream` instances with an `encoding` getter, see [TextEncoderStream](https://developer.mozilla.org/en-US/docs/Web/API/TextEncoderStream/TextEncoderStream) and [TextDecoderStream](https://developer.mozilla.org/en-US/docs/Web/API/TextDecoderStream/TextDecoderStream).

#### CompressionStream / DecompressionStream

Built as `TransformStream` instances (formats: `gzip`, `deflate`, `deflate-raw`), see [CompressionStream](https://developer.mozilla.org/en-US/docs/Web/API/CompressionStream/CompressionStream) and [DecompressionStream](https://developer.mozilla.org/en-US/docs/Web/API/DecompressionStream/DecompressionStream).

#### ArrayBuffer

Static: [isView](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer/isView). Getter: [byteLength](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer/byteLength). Method: [slice](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/ArrayBuffer/slice)

#### crypto / CryptoKey

`crypto`: [getRandomValues](https://developer.mozilla.org/en-US/docs/Web/API/Crypto/getRandomValues), [randomUUID](https://developer.mozilla.org/en-US/docs/Web/API/Crypto/randomUUID), `subtle`. `crypto.subtle`: [digest](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/digest), [generateKey](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/generateKey), [importKey](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/importKey), [exportKey](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/exportKey), [encrypt](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/encrypt), [decrypt](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/decrypt), [sign](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/sign), [verify](https://developer.mozilla.org/en-US/docs/Web/API/SubtleCrypto/verify). `CryptoKey` getters: [type](https://developer.mozilla.org/en-US/docs/Web/API/CryptoKey/type), [extractable](https://developer.mozilla.org/en-US/docs/Web/API/CryptoKey/extractable), [algorithm](https://developer.mozilla.org/en-US/docs/Web/API/CryptoKey/algorithm), [usages](https://developer.mozilla.org/en-US/docs/Web/API/CryptoKey/usages)

#### structuredClone

Global function, see [structuredClone](https://developer.mozilla.org/en-US/docs/Web/API/Window/structuredClone).

#### Event

Getters: [type](https://developer.mozilla.org/en-US/docs/Web/API/Event/type), [target](https://developer.mozilla.org/en-US/docs/Web/API/Event/target), [currentTarget](https://developer.mozilla.org/en-US/docs/Web/API/Event/currentTarget), [bubbles](https://developer.mozilla.org/en-US/docs/Web/API/Event/bubbles), [cancelable](https://developer.mozilla.org/en-US/docs/Web/API/Event/cancelable), [composed](https://developer.mozilla.org/en-US/docs/Web/API/Event/composed), [defaultPrevented](https://developer.mozilla.org/en-US/docs/Web/API/Event/defaultPrevented), [timeStamp](https://developer.mozilla.org/en-US/docs/Web/API/Event/timeStamp), [isTrusted](https://developer.mozilla.org/en-US/docs/Web/API/Event/isTrusted), [eventPhase](https://developer.mozilla.org/en-US/docs/Web/API/Event/eventPhase). Methods: [preventDefault](https://developer.mozilla.org/en-US/docs/Web/API/Event/preventDefault), [stopPropagation](https://developer.mozilla.org/en-US/docs/Web/API/Event/stopPropagation), [stopImmediatePropagation](https://developer.mozilla.org/en-US/docs/Web/API/Event/stopImmediatePropagation), [composedPath](https://developer.mozilla.org/en-US/docs/Web/API/Event/composedPath)

#### CustomEvent

Extends `Event`. Getter: [detail](https://developer.mozilla.org/en-US/docs/Web/API/CustomEvent/detail)

#### EventTarget

Methods: [addEventListener](https://developer.mozilla.org/en-US/docs/Web/API/EventTarget/addEventListener), [removeEventListener](https://developer.mozilla.org/en-US/docs/Web/API/EventTarget/removeEventListener), [dispatchEvent](https://developer.mozilla.org/en-US/docs/Web/API/EventTarget/dispatchEvent)

#### AbortController

Instance: [signal](https://developer.mozilla.org/en-US/docs/Web/API/AbortController/signal), [abort](https://developer.mozilla.org/en-US/docs/Web/API/AbortController/abort)

#### AbortSignal

Extends `EventTarget`. Static: [abort](https://developer.mozilla.org/en-US/docs/Web/API/AbortSignal/abort), [timeout](https://developer.mozilla.org/en-US/docs/Web/API/AbortSignal/timeout), [any](https://developer.mozilla.org/en-US/docs/Web/API/AbortSignal/any). Getters: [aborted](https://developer.mozilla.org/en-US/docs/Web/API/AbortSignal/aborted), [reason](https://developer.mozilla.org/en-US/docs/Web/API/AbortSignal/reason). Method: [throwIfAborted](https://developer.mozilla.org/en-US/docs/Web/API/AbortSignal/throwIfAborted)

#### WebSocket

Extends `EventTarget`. Getters: [readyState](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/readyState), [url](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/url), [bufferedAmount](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/bufferedAmount), [protocol](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/protocol), [binaryType](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/binaryType). Methods: [send](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/send), [close](https://developer.mozilla.org/en-US/docs/Web/API/WebSocket/close)

#### EventSource

Extends `EventTarget`. Getters: [readyState](https://developer.mozilla.org/en-US/docs/Web/API/EventSource/readyState), [url](https://developer.mozilla.org/en-US/docs/Web/API/EventSource/url), [withCredentials](https://developer.mozilla.org/en-US/docs/Web/API/EventSource/withCredentials). Method: [close](https://developer.mozilla.org/en-US/docs/Web/API/EventSource/close)

#### MessageChannel

Constructs a `{port1, port2}` pair, see [MessageChannel](https://developer.mozilla.org/en-US/docs/Web/API/MessageChannel/MessageChannel).

#### MessagePort

Extends `EventTarget`. Methods: [postMessage](https://developer.mozilla.org/en-US/docs/Web/API/MessagePort/postMessage), [start](https://developer.mozilla.org/en-US/docs/Web/API/MessagePort/start), [close](https://developer.mozilla.org/en-US/docs/Web/API/MessagePort/close)

#### MessageEvent

Extends `Event`. Getter: [data](https://developer.mozilla.org/en-US/docs/Web/API/MessageEvent/data)

#### BroadcastChannel

Extends `EventTarget`. Getter: [name](https://developer.mozilla.org/en-US/docs/Web/API/BroadcastChannel/name). Methods: [postMessage](https://developer.mozilla.org/en-US/docs/Web/API/BroadcastChannel/postMessage), [close](https://developer.mozilla.org/en-US/docs/Web/API/BroadcastChannel/close)

#### Worker / self

Extends `EventTarget`. Instance: [postMessage](https://developer.mozilla.org/en-US/docs/Web/API/Worker/postMessage), [terminate](https://developer.mozilla.org/en-US/docs/Web/API/Worker/terminate). Worker-scope `self`: [postMessage](https://developer.mozilla.org/en-US/docs/Web/API/DedicatedWorkerGlobalScope/postMessage), [close](https://developer.mozilla.org/en-US/docs/Web/API/DedicatedWorkerGlobalScope/close)

#### performance

Methods: [now](https://developer.mozilla.org/en-US/docs/Web/API/Performance/now), [mark](https://developer.mozilla.org/en-US/docs/Web/API/Performance/mark), [measure](https://developer.mozilla.org/en-US/docs/Web/API/Performance/measure), [getEntries](https://developer.mozilla.org/en-US/docs/Web/API/Performance/getEntries), [getEntriesByName](https://developer.mozilla.org/en-US/docs/Web/API/Performance/getEntriesByName), [getEntriesByType](https://developer.mozilla.org/en-US/docs/Web/API/Performance/getEntriesByType), [clearMarks](https://developer.mozilla.org/en-US/docs/Web/API/Performance/clearMarks), [clearMeasures](https://developer.mozilla.org/en-US/docs/Web/API/Performance/clearMeasures)

#### navigator

Data properties: [hardwareConcurrency](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/hardwareConcurrency), [userAgent](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/userAgent), [language](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/language), [languages](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/languages), [platform](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/platform)

#### URL / URLSearchParams

`URL`: [href](https://developer.mozilla.org/en-US/docs/Web/API/URL/href), [protocol](https://developer.mozilla.org/en-US/docs/Web/API/URL/protocol), [username](https://developer.mozilla.org/en-US/docs/Web/API/URL/username), [password](https://developer.mozilla.org/en-US/docs/Web/API/URL/password), [hostname](https://developer.mozilla.org/en-US/docs/Web/API/URL/hostname), [port](https://developer.mozilla.org/en-US/docs/Web/API/URL/port), [host](https://developer.mozilla.org/en-US/docs/Web/API/URL/host), [pathname](https://developer.mozilla.org/en-US/docs/Web/API/URL/pathname), [search](https://developer.mozilla.org/en-US/docs/Web/API/URL/search), [hash](https://developer.mozilla.org/en-US/docs/Web/API/URL/hash), [origin](https://developer.mozilla.org/en-US/docs/Web/API/URL/origin), [searchParams](https://developer.mozilla.org/en-US/docs/Web/API/URL/searchParams), [toString](https://developer.mozilla.org/en-US/docs/Web/API/URL/toString), [toJSON](https://developer.mozilla.org/en-US/docs/Web/API/URL/toJSON). `URLSearchParams`: [append](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/append), [delete](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/delete), [get](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/get), [getAll](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/getAll), [has](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/has), [set](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/set), [sort](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/sort), [forEach](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/forEach), [keys](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/keys), [values](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/values), [entries](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/entries), [toString](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams/toString)

#### WebAssembly

Namespace functions: [compile](https://developer.mozilla.org/en-US/docs/Web/API/WebAssembly/compile), [instantiate](https://developer.mozilla.org/en-US/docs/Web/API/WebAssembly/instantiate), [validate](https://developer.mozilla.org/en-US/docs/Web/API/WebAssembly/validate). `WebAssembly.Memory` / `Table` / `Global` / `Module` / `Instance` are not exposed as separate constructible classes.

### Node.js modules (methods)

#### `tty`

[Node.js docs](https://nodejs.org/api/tty.html): `isatty`

#### `path`

[Node.js docs](https://nodejs.org/api/path.html): `sep`, `delimiter`, `join`, `resolve`, `normalize`, `isAbsolute`, `dirname`, `basename`, `extname`, `relative`, `parse`, `format`, `win32`, `posix`

#### `buffer`

[Node.js docs](https://nodejs.org/api/buffer.html): `Buffer.from`, `Buffer.alloc`, `Buffer.allocUnsafe`, `Buffer.isBuffer`, `Buffer.byteLength`, `Buffer.concat`, `toString`, `write`, `slice`, `equals`, `toJSON`

#### `util`

[Node.js docs](https://nodejs.org/api/util.html): `format`, `inspect`, `promisify`, `inherits`, `types`, `deprecate`, `callbackify`

#### `os`

[Node.js docs](https://nodejs.org/api/os.html): `platform`, `arch`, `type`, `release`, `homedir`, `tmpdir`, `hostname`, `cpus`, `totalmem`, `freemem`, `EOL`, `endianness`

#### `events`

[Node.js docs](https://nodejs.org/api/events.html): `on`, `addListener`, `prependListener`, `once`, `prependOnceListener`, `off`, `removeListener`, `emit`, `removeAllListeners`, `listenerCount`, `listeners`, `eventNames`, `setMaxListeners`

#### `assert`

[Node.js docs](https://nodejs.org/api/assert.html): `ok`, `equal`, `notEqual`, `strictEqual`, `notStrictEqual`, `deepEqual`, `deepStrictEqual`, `notDeepEqual`, `notDeepStrictEqual`, `throws`, `doesNotThrow`, `fail`, `match`, `doesNotMatch`, `rejects`, `doesNotReject`, `CallTracker`

#### `querystring`

[Node.js docs](https://nodejs.org/api/querystring.html): `escape`, `unescape`, `parse`, `stringify`, `decode`, `encode`

#### `perf_hooks`

[Node.js docs](https://nodejs.org/api/perf_hooks.html): `performance`

#### `dns`

[Node.js docs](https://nodejs.org/api/dns.html): `lookup`, `resolve4`, `resolve6`, `resolveMx`, `resolveTxt`, `resolveCname`, `resolveNs`, `reverse`, `Resolver`, `promises`

#### `string_decoder`

[Node.js docs](https://nodejs.org/api/string_decoder.html): `StringDecoder`

#### `url`

[Node.js docs](https://nodejs.org/api/url.html): `URL`, `URLSearchParams`, `parse`, `format`, `resolve`

#### `zlib`

[Node.js docs](https://nodejs.org/api/zlib.html): `gzipSync`, `gunzipSync`, `deflateSync`, `inflateSync`, `deflateRawSync`, `inflateRawSync`, `gzip`, `gunzip`, `deflate`, `inflate`, `deflateRaw`, `inflateRaw`

#### `crypto`

[Node.js docs](https://nodejs.org/api/crypto.html): `createHash`, `createHmac`, `createCipheriv`, `createDecipheriv`, `createSign`, `createVerify`, `randomBytes`, `randomUUID`, `randomInt`, `timingSafeEqual`, `pbkdf2Sync`, `pbkdf2`, `generateKeyPairSync`, `generateKeyPair`

#### `stream`

[Node.js docs](https://nodejs.org/api/stream.html): `Readable`, `Writable`, `Duplex`, `Transform`, `PassThrough`, `pipeline`, `finished`

#### `child_process`

[Node.js docs](https://nodejs.org/api/child_process.html): `spawn`, `exec`, `execSync`, `spawnSync`, `execFile`, `fork`

#### `net`

[Node.js docs](https://nodejs.org/api/net.html): `createServer`, `connect`, `createConnection`

#### `http`

[Node.js docs](https://nodejs.org/api/http.html): `createServer`, `request`, `get`, `Agent`, `globalAgent`, `METHODS`, `STATUS_CODES`

#### `https`

[Node.js docs](https://nodejs.org/api/https.html): `createServer`, `request`, `get`, `Agent`, `globalAgent`

#### `tls`

[Node.js docs](https://nodejs.org/api/tls.html): `createServer`, `connect`

#### `readline`

[Node.js docs](https://nodejs.org/api/readline.html): `createInterface`

#### `worker_threads`

[Node.js docs](https://nodejs.org/api/worker_threads.html): `MessageChannel`, `Worker`, `isMainThread`, `threadId`, `workerData`, `parentPort`

#### `cluster`

[Node.js docs](https://nodejs.org/api/cluster.html): `isPrimary`, `isMaster`, `isWorker`, `workers`, `fork`

#### `repl`

[Node.js docs](https://nodejs.org/api/repl.html): `start`

#### `timers`

[Node.js docs](https://nodejs.org/api/timers.html): `setTimeout`, `clearTimeout`, `setInterval`, `clearInterval`, `setImmediate`, `clearImmediate`, `promises`

#### `dgram`

[Node.js docs](https://nodejs.org/api/dgram.html): `createSocket`

#### `http2`

[Node.js docs](https://nodejs.org/api/http2.html): `connect`, `request`, `createServer`, `createSecureServer`, `constants`

#### `v8`

[Node.js docs](https://nodejs.org/api/v8.html): `getHeapStatistics`, `getHeapSpaceStatistics`, `setFlagsFromString`, `serialize`, `deserialize`, `writeHeapSnapshot`

#### `module`

[Node.js docs](https://nodejs.org/api/module.html): `builtinModules`, `isBuiltin`, `createRequire`

#### `diagnostics_channel`

[Node.js docs](https://nodejs.org/api/diagnostics_channel.html): `publish`, `subscribe`, `unsubscribe`, `channel`

#### `async_hooks`

[Node.js docs](https://nodejs.org/api/async_hooks.html): `createHook`, `enable`, `disable`, `executionAsyncId`, `triggerAsyncId`, `executionAsyncResource`, `AsyncLocalStorage`

#### `inspector`

[Node.js docs](https://nodejs.org/api/inspector.html): `open`, `close`, `url`, `waitForDebugger`, `Session`

#### `trace_events`

[Node.js docs](https://nodejs.org/api/trace_events.html): `createTracing`, `getEnabledCategories`


### Benchmarks

The `bench/` suite in the V6 repository runs 40 fixtures, each covering a different language or standard library feature, under `node` and under `v6` in its default persistent-process mode.

V6's persistent process is faster than Node on 25 of these 40 fixtures. A standalone AOT JAR is slower than Node on all 40, since each run pays a full `java -jar` startup cost instead of reusing a warm process, so that mode is left out of the comparison entirely rather than mixed into a same-scale benchmark against two already-warm processes.

Measured with `hyperfine`, using 6 warmup runs followed by a minimum of 5 timed runs per command, on an Intel Core i7-4800MQ (Haswell, 4 cores, 8 threads) with 8 GB of RAM, against `node v26.5.0` and `v6 v0.2.0`.

See the [introductory blog post](../blog/intro/index.html) for the full breakdown by category, with charts, and the [v0.2.0 release notes](../blog/v0-2-0/index.html) for the WebAssembly benchmark suite against `wasmtime`.

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

`Proxy`, `Reflect`, `eval`, the `vm` module, the dynamic `import()` expression and typed array types other than `Uint8Array` are not implemented. WebAssembly SIMD is not implemented; a module that uses it fails to load.

### V6

V6 is a JavaScript runtime built on the JVM. It compiles JavaScript directly to JVM bytecode and runs it with a persistent warm process so repeated runs start fast. V6 also runs Java code from JavaScript through a built-in `java:` import scheme and can compile a script into a standalone executable JAR.

### Installation

Download the latest build from the [releases page](https://github.com/v6js/v6/releases/latest).

Each release ships two variants per platform.

#### Developer build

A small download that uses a JDK already installed on your machine through `JAVA_HOME`. Pick this if you already have a JDK.

#### Portable build

A self-contained download that bundles a full JDK. Pick this if you want to run V6 without installing a JDK first.

Available platforms:

- x86_64-linux-gnu
- aarch64-linux-gnu
- x86_64-windows-gnu
- x86_64-macos
- aarch64-macos

Unzip the archive and add the folder to your `PATH`.

### Quick start

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

### CLI

#### Flags

- `-e, --eval <code>` evaluate code and exit
- `-o <output.jar>` compile to a standalone jar
- `-cp, --classpath <path>` extra Java classpath for `java:` imports
- `--no-daemon` skip the persistent warm process
- `--color` force colored output
- `--no-color` disable colored output
- `-v, --version` print the version
- `-h, --help` print help

#### REPL commands

- `.help` show REPL commands
- `.exit` exit the REPL
- `.break` cancel the current multi-line input
- `.clear` reset the session
- `.history` print input history
- `.save <file>` save session source to a file
- `.load <file>` load and run a file in the session

Press Ctrl+C once to abort the current expression and again to exit. Ctrl+D exits directly.

### Java interop

Import a Java class with the `java:` scheme and use it like a normal JavaScript value.

```js
import ArrayList from "java:java.util.ArrayList";

const list = new ArrayList();
list.add("hello");
console.log(list.get(0));
```

### AOT compilation

Compile a script and its dependencies into a single runnable jar with `-o`.

```
v6 app.js -o app.jar
java -jar app.jar
```

### Global APIs

- [x] console
- [x] globalThis / global
- [x] Math
- [x] JSON
- [x] Object
- [x] Array
- [x] String
- [x] Number
- [x] Boolean
- [x] Date
- [x] RegExp
- [x] Function
- [x] Symbol
- [x] Map / Set
- [x] WeakMap / WeakSet
- [x] Promise
- [x] Error and its subclasses
- [x] BigInt
- [x] Buffer
- [x] Uint8Array
- [x] URL / URLSearchParams
- [x] setTimeout / setInterval / setImmediate / queueMicrotask
- [x] atob / btoa
- [x] encodeURIComponent / decodeURIComponent / encodeURI / decodeURI
- [ ] Proxy
- [ ] Reflect
- [ ] WeakRef / FinalizationRegistry
- [ ] other typed array types (Int8Array, Float32Array, DataView and so on)

### Isomorphic Web APIs

- [x] fetch
- [x] Headers / Request / Response
- [x] Blob / File / FormData
- [x] ReadableStream / WritableStream / TransformStream
- [x] TextEncoder / TextDecoder
- [x] TextEncoderStream / TextDecoderStream
- [x] CompressionStream / DecompressionStream
- [x] ArrayBuffer
- [x] crypto / CryptoKey
- [x] structuredClone
- [x] Event / CustomEvent / EventTarget
- [x] AbortController / AbortSignal
- [x] WebSocket
- [x] EventSource
- [x] MessageChannel / MessagePort / MessageEvent
- [x] BroadcastChannel
- [x] Worker / self
- [x] performance
- [x] navigator

### Node.js compatibility

- [x] path
- [x] buffer
- [x] util
- [x] os
- [x] tty
- [x] fs
- [x] events
- [x] assert
- [x] querystring
- [x] perf_hooks
- [x] dns
- [x] string_decoder
- [x] url
- [x] zlib
- [x] crypto
- [x] stream
- [x] child_process
- [x] net
- [x] http
- [x] https
- [x] tls
- [x] readline
- [x] worker_threads
- [x] cluster
- [x] repl
- [x] timers
- [x] dgram
- [x] http2
- [x] v8
- [x] module
- [x] diagnostics_channel
- [x] async_hooks
- [x] inspector
- [x] trace_events
- [ ] vm

### License

Apache License 2.0. See [LICENSE](LICENSE).

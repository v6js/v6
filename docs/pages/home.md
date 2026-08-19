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

### Why V6

A `.js` file compiles to real `.class` files. A JavaScript function is a JVM method. A JavaScript value is a JVM object. Both share one process, one heap and one garbage collector, so a Java application can pass an object into JavaScript and hold onto it from a closure exactly like it would if both were written in Java, because at the bytecode level, they both are.

Read more about the design in the [introductory blog post](blog/intro/index.html), or head to the [docs](docs/index.html) for installation, the CLI, module resolution and Java interop.

public final class V6ModuleModule {
  private V6ModuleModule() {
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final String[] BUILTIN_NAMES = {
      "path",
      "util",
      "os",
      "fs",
      "events",
      "assert",
      "querystring",
      "perf_hooks",
      "dns",
      "string_decoder",
      "url",
      "zlib",
      "crypto",
      "stream",
      "child_process",
      "net",
      "http",
      "https",
      "tls",
      "readline",
      "worker_threads",
      "cluster",
      "repl",
      "timers",
      "dgram",
      "http2",
      "buffer",
      "module",
      "v8",
      "diagnostics_channel",
      "async_hooks",
  };

  public static V6Object build() {
    V6Object o = new V6Object();

    V6Array builtinModules = new V6Array();
    for (String n : BUILTIN_NAMES)
      builtinModules.push(str(n));
    o.set("builtinModules", objValue(builtinModules));

    o.set("isBuiltin", fn((thisArg, args) -> {
            String name = V6Value.argAt(args, 0).toString();
            for (String n : BUILTIN_NAMES)
              if (n.equals(name))
                return new V6Value(V6Value.TAG_BOOL, 1, null);
            return new V6Value(V6Value.TAG_BOOL, 0, null);
          }));

    o.set("createRequire", fn((thisArg, args) -> {
            throw new V6Throw(str("module.createRequire is not supported (v6 " +
                                  "resolves 'require' statically at "
                                  + "compile time; there is no runtime " +
                                    "module loader to hand back dynamically)"));
          }));

    return o;
  }
}

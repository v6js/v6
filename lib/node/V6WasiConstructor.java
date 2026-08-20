public final class V6WasiConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public static V6Object build() {
    V6Object o = new V6Object();
    o.set("WASI", new V6Value(V6Value.TAG_OBJ, 0, new V6WasiConstructor()));
    return o;
  }

  public V6WasiConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6WasiObject o = new V6WasiObject();
    o.setProto(PROTOTYPE);

    V6Value opts = V6Value.argAt(args, 0);
    V6Value argsVal = opts.getProp("args");
    if (argsVal.tag() == V6Value.TAG_OBJ) {
      int len = (int)argsVal.getProp("length").toNumber();
      for (int i = 0; i < len; i++)
        o.args.add(argsVal.getProp(String.valueOf(i)).toString());
    }
    V6Value envVal = opts.getProp("env");
    if (envVal.tag() == V6Value.TAG_OBJ && envVal.ref() instanceof
                                               V6Object envObj) {
      for (String key : envObj.keySet())
        o.envPairs.add(key + "=" + envObj.get(key).toString());
    }
    o.returnOnExit = opts.getProp("returnOnExit").truthy();

    o.set("wasiImport", objValue(buildWasiImport(o)));

    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value objValue(Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6WasiObject self(V6Value t) {
    return (V6WasiObject)t.ref();
  }

  static V6Object buildWasiImport(V6WasiObject o) {
    V6Object imp = new V6Object();
    imp.set("args_sizes_get", fn((t, a) -> o.argsSizesGet(a)));
    imp.set("args_get", fn((t, a) -> o.argsGet(a)));
    imp.set("environ_sizes_get", fn((t, a) -> o.environSizesGet(a)));
    imp.set("environ_get", fn((t, a) -> o.environGet(a)));
    imp.set("proc_exit", fn((t, a) -> o.procExit(a)));
    imp.set("fd_write", fn((t, a) -> o.fdWrite(a)));
    imp.set("fd_read", fn((t, a) -> o.fdRead(a)));
    imp.set("fd_close", fn((t, a) -> o.fdClose(a)));
    imp.set("fd_seek", fn((t, a) -> o.fdSeek(a)));
    imp.set("fd_fdstat_get", fn((t, a) -> o.fdFdstatGet(a)));
    imp.set("fd_prestat_get", fn((t, a) -> o.fdPrestatGet(a)));
    imp.set("clock_time_get", fn((t, a) -> o.clockTimeGet(a)));
    imp.set("random_get", fn((t, a) -> o.randomGet(a)));
    return imp;
  }

  private static V6Value callStart(V6Value thisArg, V6Value[] args,
                                   String exportName) {
    V6WasiObject o = self(thisArg);
    V6Value instanceVal = V6Value.argAt(args, 0);
    o.hookMemory(instanceVal);
    V6Value exportsVal = instanceVal.getProp("exports");
    V6Value entryFn = exportsVal.getProp(exportName);
    if (entryFn.tag() != V6Value.TAG_FUNC)
      throw new RuntimeException("TypeError: wasm module has no exported '" +
                                 exportName + "' function");
    try {
      entryFn.call(V6Value.UNDEF, new V6Value[0]);
    } catch (V6ProcessExit e) {
      if (!o.returnOnExit)
        throw e;
      return new V6Value(V6Value.TAG_NUM, e.code, null);
    }
    return UNDEF;
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.set("start", fn((t, a) -> callStart(t, a, "_start")));
    o.set("initialize", fn((t, a) -> callStart(t, a, "_initialize")));
    return o;
  }
}

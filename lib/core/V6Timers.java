public final class V6Timers {
  private V6Timers() {
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value[] restArgs(V6Value[] args, int from) {
    if (args.length <= from)
      return new V6Value[0];
    V6Value[] out = new V6Value[args.length - from];
    System.arraycopy(args, from, out, 0, out.length);
    return out;
  }

  private static void clearById(V6Value[] a) {
    if (a.length > 0 && a[0].tag() == V6Value.TAG_NUM)
      V6EventLoop.cancel((long)a[0].toNumber());
  }

  public static final V6Value SET_TIMEOUT = fn((t, a) -> {
    V6Callable cb = V6Value.argAt(a, 0).asCallable();
    double delay = a.length > 1 ? a[1].toNumber() : 0;
    long id = V6EventLoop.schedule(cb, delay, 0, restArgs(a, 2));
    return num(id);
  });

  public static final V6Value CLEAR_TIMEOUT = fn((t, a) -> {
    clearById(a);
    return UNDEF;
  });

  public static final V6Value SET_INTERVAL = fn((t, a) -> {
    V6Callable cb = V6Value.argAt(a, 0).asCallable();
    double delay = a.length > 1 ? a[1].toNumber() : 0;
    long id =
        V6EventLoop.schedule(cb, delay, Math.max(1, delay), restArgs(a, 2));
    return num(id);
  });

  public static final V6Value CLEAR_INTERVAL = fn((t, a) -> {
    clearById(a);
    return UNDEF;
  });

  public static final V6Value SET_IMMEDIATE = fn((t, a) -> {
    V6Callable cb = V6Value.argAt(a, 0).asCallable();
    long id = V6EventLoop.schedule(cb, 0, 0, restArgs(a, 1));
    return num(id);
  });

  public static final V6Value CLEAR_IMMEDIATE = fn((t, a) -> {
    clearById(a);
    return UNDEF;
  });

  public static final V6Value QUEUE_MICROTASK = fn((t, a) -> {
    V6Callable cb = V6Value.argAt(a, 0).asCallable();
    V6MicrotaskQueue.enqueue(() -> cb.call(UNDEF, new V6Value[0]));
    return UNDEF;
  });

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Object buildPromisesModule() {
    V6Object p = new V6Object();
    p.set("setTimeout", fn((t, a) -> {
            double delay = a.length > 0 ? a[0].toNumber() : 0;
            V6Value value = a.length > 1 ? a[1] : UNDEF;
            V6Promise promise = new V6Promise();
            V6EventLoop.schedule((t2, a2) -> {
              promise.resolve(value);
              return UNDEF;
            }, delay, 0, new V6Value[0]);
            return objValue(promise);
          }));
    p.set("setImmediate", fn((t, a) -> {
            V6Value value = a.length > 0 ? a[0] : UNDEF;
            V6Promise promise = new V6Promise();
            V6EventLoop.schedule((t2, a2) -> {
              promise.resolve(value);
              return UNDEF;
            }, 0, 0, new V6Value[0]);
            return objValue(promise);
          }));
    return p;
  }

  public static V6Object buildModule() {
    V6Object o = new V6Object();
    o.set("setTimeout", SET_TIMEOUT);
    o.set("clearTimeout", CLEAR_TIMEOUT);
    o.set("setInterval", SET_INTERVAL);
    o.set("clearInterval", CLEAR_INTERVAL);
    o.set("setImmediate", SET_IMMEDIATE);
    o.set("clearImmediate", CLEAR_IMMEDIATE);
    o.set("promises", objValue(buildPromisesModule()));
    return o;
  }
}

public final class V6StreamMethods {
  private V6StreamMethods() {}

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  static void emit(V6Value t, String event, V6Value... extra) {
    V6Value[] args = new V6Value[extra.length + 1];
    args[0] = str(event);
    System.arraycopy(extra, 0, args, 1, extra.length);
    t.getProp("emit").asCallable().call(t, args);
  }

  static void on(V6Value t, String event, V6Callable cb) {
    t.getProp("on").asCallable().call(t, new V6Value[] {str(event), fn(cb)});
  }

  static V6Callable lastCallback(V6Value[] a) {
    return a.length > 0 && a[a.length - 1].tag() == V6Value.TAG_FUNC
        ? a[a.length - 1].asCallable()
        : null;
  }

  static void installReadable(V6Object o) {
    o.set("push", fn((t, a) -> {
            V6Value chunk = V6Value.argAt(a, 0);
            if (chunk.tag() == V6Value.TAG_NULL || chunk.isUndefined()) {
              emit(t, "end");
              return bool(false);
            }
            emit(t, "data", chunk);
            return bool(true);
          }));

    o.set("pipe", fn((t, a) -> {
            V6Value dest = V6Value.argAt(a, 0);
            on(t, "data",
               (t2, a2)
                   -> dest.getProp("write").asCallable().call(
                       dest, new V6Value[] {V6Value.argAt(a2, 0)}));
            on(t, "end", (t2, a2) -> {
              V6Value endFn = dest.getProp("end");
              if (endFn.tag() == V6Value.TAG_FUNC)
                endFn.asCallable().call(dest, new V6Value[0]);
              return UNDEF;
            });
            return dest;
          }));

    o.set("pause", fn((t, a) -> t));
    o.set("resume", fn((t, a) -> t));
    o.set("isPaused", fn((t, a) -> bool(false)));
    o.set("read", fn((t, a) -> new V6Value(V6Value.TAG_NULL, 0, null)));
    o.set("destroy", fn((t, a) -> {
            emit(t, "close");
            return t;
          }));
  }

  static void installWritable(V6Object o) {
    o.set("write", fn((t, a) -> {
            V6Value writeFn = t.getProp("_write");
            V6Value chunk = V6Value.argAt(a, 0);
            V6Callable cb = lastCallback(a);
            if (writeFn.tag() == V6Value.TAG_FUNC) {
              writeFn.asCallable().call(
                  t, new V6Value[] {chunk, str("utf8"),
                                    fn((t2, a2) -> {
                                      if (cb != null)
                                        cb.call(UNDEF, new V6Value[0]);
                                      return UNDEF;
                                    })});
            } else if (cb != null) {
              cb.call(UNDEF, new V6Value[0]);
            }
            return bool(true);
          }));

    o.set("end", fn((t, a) -> {
            if (a.length > 0 && a[0].tag() != V6Value.TAG_FUNC)
              t.getProp("write").asCallable().call(t, new V6Value[] {a[0]});
            emit(t, "finish");
            V6Callable cb = lastCallback(a);
            if (cb != null)
              cb.call(UNDEF, new V6Value[0]);
            return UNDEF;
          }));

    o.set("destroy", fn((t, a) -> {
            emit(t, "close");
            return t;
          }));
  }
}

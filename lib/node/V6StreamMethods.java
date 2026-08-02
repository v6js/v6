public final class V6StreamMethods {
  private V6StreamMethods() {
  }

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

  private static V6StreamQueue queueOf(V6Object self) {
    V6Value v = self.get("_queueHolder");
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6StreamQueue)
      return (V6StreamQueue)v.ref();
    V6StreamQueue q = new V6StreamQueue();
    self.set("_queueHolder", new V6Value(V6Value.TAG_OBJ, 0, q));
    return q;
  }

  private static void doWrite(V6Value t, V6Object self, V6StreamQueue q,
                              V6Value chunk, V6Callable userCb) {
    self.set("_writing", bool(true));
    V6Value writeFn = t.getProp("_write");
    V6Callable completion = (t2, a2) -> {
      if (userCb != null)
        userCb.call(UNDEF, new V6Value[0]);
      if (!q.pending.isEmpty()) {
        Object[] next = q.pending.poll();
        doWrite(t, self, q, (V6Value)next[0], (V6Callable)next[1]);
      } else {
        self.set("_writing", bool(false));
        emit(t, "drain");
      }
      return UNDEF;
    };
    if (writeFn.tag() == V6Value.TAG_FUNC) {
      writeFn.asCallable().call(
          t, new V6Value[] {chunk, str("utf8"), fn(completion)});
    } else {
      completion.call(UNDEF, new V6Value[0]);
    }
  }

  static void installWritable(V6Object o) {
    o.set("write", fn((t, a) -> {
            V6Object self = (V6Object)t.ref();
            V6Value chunk = V6Value.argAt(a, 0);
            V6Callable cb = lastCallback(a);
            V6StreamQueue q = queueOf(self);

            if (self.get("_writing").truthy()) {
              q.pending.add(new Object[] {chunk, cb});
              return bool(false);
            }
            doWrite(t, self, q, chunk, cb);
            return bool(q.pending.isEmpty());
          }));

    o.set("end", fn((t, a) -> {
            V6Object self = (V6Object)t.ref();
            V6Callable cb = lastCallback(a);
            if (a.length > 0 && a[0].tag() != V6Value.TAG_FUNC)
              t.getProp("write").asCallable().call(t, new V6Value[] {a[0]});
            V6StreamQueue q = queueOf(self);
            Runnable doFinish = () -> {
              emit(t, "finish");
              if (cb != null)
                cb.call(UNDEF, new V6Value[0]);
            };
            if (q.pending.isEmpty() && !self.get("_writing").truthy()) {
              doFinish.run();
            } else {
              on(t, "drain", (t2, a2) -> {
                doFinish.run();
                return UNDEF;
              });
            }
            return UNDEF;
          }));

    o.set("destroy", fn((t, a) -> {
            emit(t, "close");
            return t;
          }));
  }

  private static void wireCompletion(java.util.List<V6Value> streams,
                                     V6Callable cb) {
    boolean[] called = {false};
    V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);
    for (V6Value s : streams) {
      on(s, "error", (t, a) -> {
        if (!called[0]) {
          called[0] = true;
          cb.call(UNDEF, new V6Value[] {V6Value.argAt(a, 0)});
        }
        return UNDEF;
      });
    }
    V6Value last = streams.get(streams.size() - 1);
    V6Callable onDone = (t, a) -> {
      if (!called[0]) {
        called[0] = true;
        cb.call(UNDEF, new V6Value[] {NUL});
      }
      return UNDEF;
    };
    on(last, "finish", onDone);
    on(last, "end", onDone);
    on(last, "close", onDone);
  }

  static V6Value pipelineImpl(V6Value[] args) {
    V6Callable cb = null;
    java.util.List<V6Value> streams = new java.util.ArrayList<>();
    for (V6Value a : args) {
      if (a.tag() == V6Value.TAG_FUNC)
        cb = a.asCallable();
      else
        streams.add(a);
    }
    for (int i = 0; i + 1 < streams.size(); i++)
      streams.get(i).getProp("pipe").asCallable().call(
          streams.get(i), new V6Value[] {streams.get(i + 1)});
    if (!streams.isEmpty() && cb != null)
      wireCompletion(streams, cb);
    return streams.isEmpty() ? UNDEF : streams.get(streams.size() - 1);
  }

  static V6Value finishedImpl(V6Value[] args) {
    V6Value stream = V6Value.argAt(args, 0);
    V6Callable cb = args.length > 1 && args[1].tag() == V6Value.TAG_FUNC
                        ? args[1].asCallable()
                        : null;
    if (cb != null)
      wireCompletion(java.util.List.of(stream), cb);
    return UNDEF;
  }
}

public final class V6PromiseConstructor extends V6Object implements V6NativeConstructor {
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Array collect(V6Value iterable) {
    V6Array items = new V6Array();
    items.pushAll(iterable);
    return items;
  }

  public V6PromiseConstructor() {
    set("resolve", fn((t, a) -> objValue(V6Promise.resolved(V6Value.argAt(a, 0)))));
    set("reject", fn((t, a) -> objValue(V6Promise.rejected(V6Value.argAt(a, 0)))));

    set("all", fn((t, a) -> {
          V6Array items = collect(V6Value.argAt(a, 0));
          int n = (int)items.get("length").num();
          V6Promise result = new V6Promise();
          if (n == 0) {
            result.resolve(objValue(new V6Array()));
            return objValue(result);
          }
          V6Value[] results = new V6Value[n];
          int[] remaining = {n};
          boolean[] settled = {false};
          for (int i = 0; i < n; i++) {
            final int idx = i;
            V6Promise p = V6Promise.resolved(items.get(Integer.toString(i)));
            p.addCallbacks(
                v -> {
                  if (settled[0])
                    return;
                  results[idx] = v;
                  if (--remaining[0] == 0) {
                    settled[0] = true;
                    V6Array arr = new V6Array();
                    for (V6Value r : results)
                      arr.push(r);
                    result.resolve(objValue(arr));
                  }
                },
                err -> {
                  if (!settled[0]) {
                    settled[0] = true;
                    result.reject(err);
                  }
                });
          }
          return objValue(result);
        }));

    set("race", fn((t, a) -> {
          V6Array items = collect(V6Value.argAt(a, 0));
          int n = (int)items.get("length").num();
          V6Promise result = new V6Promise();
          boolean[] settled = {false};
          for (int i = 0; i < n; i++) {
            V6Promise p = V6Promise.resolved(items.get(Integer.toString(i)));
            p.addCallbacks(
                v -> {
                  if (!settled[0]) {
                    settled[0] = true;
                    result.resolve(v);
                  }
                },
                err -> {
                  if (!settled[0]) {
                    settled[0] = true;
                    result.reject(err);
                  }
                });
          }
          return objValue(result);
        }));

    set("allSettled", fn((t, a) -> {
          V6Array items = collect(V6Value.argAt(a, 0));
          int n = (int)items.get("length").num();
          V6Promise result = new V6Promise();
          if (n == 0) {
            result.resolve(objValue(new V6Array()));
            return objValue(result);
          }
          V6Value[] results = new V6Value[n];
          int[] remaining = {n};
          for (int i = 0; i < n; i++) {
            final int idx = i;
            V6Promise p = V6Promise.resolved(items.get(Integer.toString(i)));
            p.addCallbacks(
                v -> {
                  V6Object o = new V6Object();
                  o.set("status", new V6Value(V6Value.TAG_STR, 0, "fulfilled"));
                  o.set("value", v);
                  results[idx] = objValue(o);
                  if (--remaining[0] == 0) {
                    V6Array arr = new V6Array();
                    for (V6Value r : results)
                      arr.push(r);
                    result.resolve(objValue(arr));
                  }
                },
                err -> {
                  V6Object o = new V6Object();
                  o.set("status", new V6Value(V6Value.TAG_STR, 0, "rejected"));
                  o.set("reason", err);
                  results[idx] = objValue(o);
                  if (--remaining[0] == 0) {
                    V6Array arr = new V6Array();
                    for (V6Value r : results)
                      arr.push(r);
                    result.resolve(objValue(arr));
                  }
                });
          }
          return objValue(result);
        }));

    set("any", fn((t, a) -> {
          V6Array items = collect(V6Value.argAt(a, 0));
          int n = (int)items.get("length").num();
          V6Promise result = new V6Promise();
          if (n == 0) {
            result.reject(new V6Value(V6Value.TAG_STR, 0, "AggregateError: All promises were rejected"));
            return objValue(result);
          }
          V6Value[] errors = new V6Value[n];
          int[] remaining = {n};
          boolean[] settled = {false};
          for (int i = 0; i < n; i++) {
            final int idx = i;
            V6Promise p = V6Promise.resolved(items.get(Integer.toString(i)));
            p.addCallbacks(
                v -> {
                  if (!settled[0]) {
                    settled[0] = true;
                    result.resolve(v);
                  }
                },
                err -> {
                  errors[idx] = err;
                  if (--remaining[0] == 0 && !settled[0]) {
                    result.reject(new V6Value(V6Value.TAG_STR, 0,
                                              "AggregateError: All promises were rejected"));
                  }
                });
          }
          return objValue(result);
        }));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Promise p = new V6Promise();
    V6Value executor = V6Value.argAt(args, 0);
    if (executor.tag() == V6Value.TAG_FUNC) {
      V6Callable exec = executor.asCallable();
      V6Value resolveFn = fn((t, a) -> {
        p.resolve(V6Value.argAt(a, 0));
        return UNDEF;
      });
      V6Value rejectFn = fn((t, a) -> {
        p.reject(V6Value.argAt(a, 0));
        return UNDEF;
      });
      try {
        exec.call(UNDEF, new V6Value[] {resolveFn, rejectFn});
      } catch (V6Throw e) {
        p.reject(e.value);
      } catch (RuntimeException e) {
        p.reject(new V6Value(V6Value.TAG_STR, 0, String.valueOf(e.getMessage())));
      }
    }
    return objValue(p);
  }
}

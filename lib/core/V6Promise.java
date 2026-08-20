import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

public final class V6Promise extends V6Object {
  private static final int PENDING = 0;
  private static final int FULFILLED = 1;
  private static final int REJECTED = 2;
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private int state = PENDING;
  private V6Value value = UNDEF;
  private final List<Consumer<V6Value>> onFulfill = new ArrayList<>();
  private final List<Consumer<V6Value>> onReject = new ArrayList<>();

  public V6Promise() {
    setProto(V6Builtins.PROMISE_PROTOTYPE);
  }

  public static V6Promise resolved(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Promise)
      return (V6Promise)v.ref();
    V6Promise p = new V6Promise();
    p.resolve(v);
    return p;
  }

  public static V6Promise rejected(V6Value v) {
    V6Promise p = new V6Promise();
    p.reject(v);
    return p;
  }

  public void resolve(V6Value v) {
    if (state != PENDING)
      return;
    if (v.tag() == V6Value.TAG_OBJ && v.ref() == this) {
      reject(new V6Value(V6Value.TAG_STR, 0, "TypeError: chaining cycle"));
      return;
    }
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Promise) {
      V6Promise inner = (V6Promise)v.ref();
      inner.addCallbacks(this::resolve, this::reject);
      return;
    }
    if (v.tag() == V6Value.TAG_OBJ) {
      V6Value thenFn = ((V6Object)v.ref()).get("then");
      if (thenFn.tag() == V6Value.TAG_FUNC) {
        V6Callable then = thenFn.asCallable();
        V6MicrotaskQueue.enqueue(() -> {
          try {
            then.call(
                v,
                new V6Value[] {
                    new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(t, a) -> {
                      resolve(V6Value.argAt(a, 0));
                      return UNDEF;
                    }), new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(t, a) -> {
                      reject(V6Value.argAt(a, 0));
                      return UNDEF;
                    })});
          } catch (V6Throw e) {
            reject(e.value);
          } catch (V6ProcessExit e) {
            throw e;
          } catch (RuntimeException e) {
            reject(new V6Value(V6Value.TAG_STR, 0,
                               String.valueOf(e.getMessage())));
          }
        });
        return;
      }
    }
    state = FULFILLED;
    value = v;
    flush();
  }

  public void reject(V6Value v) {
    if (state != PENDING)
      return;
    state = REJECTED;
    value = v;
    flush();
  }

  private void flush() {
    List<Consumer<V6Value>> cbs = state == FULFILLED ? onFulfill : onReject;
    V6Value v = value;
    for (Consumer<V6Value> cb : cbs)
      V6MicrotaskQueue.enqueue(() -> cb.accept(v));
    onFulfill.clear();
    onReject.clear();
  }

  public void addCallbacks(Consumer<V6Value> onOk, Consumer<V6Value> onErr) {
    if (state == PENDING) {
      onFulfill.add(onOk);
      onReject.add(onErr);
    } else if (state == FULFILLED) {
      V6Value v = value;
      V6MicrotaskQueue.enqueue(() -> onOk.accept(v));
    } else {
      V6Value v = value;
      V6MicrotaskQueue.enqueue(() -> onErr.accept(v));
    }
  }

  public V6Value then(V6Value onFulfilled, V6Value onRejected) {
    V6Promise result = new V6Promise();
    boolean hasOk = onFulfilled.tag() == V6Value.TAG_FUNC;
    boolean hasErr = onRejected.tag() == V6Value.TAG_FUNC;
    addCallbacks(
        v
        -> {
          if (hasOk) {
            try {
              result.resolve(
                  onFulfilled.asCallable().call(UNDEF, new V6Value[] {v}));
            } catch (V6Throw e) {
              result.reject(e.value);
            } catch (V6ProcessExit e) {
              throw e;
            } catch (RuntimeException e) {
              result.reject(new V6Value(V6Value.TAG_STR, 0,
                                        String.valueOf(e.getMessage())));
            }
          } else {
            result.resolve(v);
          }
        },
        v -> {
          if (hasErr) {
            try {
              result.resolve(
                  onRejected.asCallable().call(UNDEF, new V6Value[] {v}));
            } catch (V6Throw e) {
              result.reject(e.value);
            } catch (V6ProcessExit e) {
              throw e;
            } catch (RuntimeException e) {
              result.reject(new V6Value(V6Value.TAG_STR, 0,
                                        String.valueOf(e.getMessage())));
            }
          } else {
            result.reject(v);
          }
        });
    return new V6Value(V6Value.TAG_OBJ, 0, result);
  }
}

import java.util.concurrent.SynchronousQueue;

public final class V6Generator extends V6Object {
  static final int RESUME = 0;
  static final int THROW = 1;
  static final int RETURN = 2;
  static final int YIELD = 0;
  static final int DONE = 1;
  static final int ERROR = 2;
  static final int AWAIT_YIELD = 3;

  private static final ThreadLocal<V6Generator> CURRENT = new ThreadLocal<>();

  private final SynchronousQueue<Object[]> toCoroutine =
      new SynchronousQueue<>();
  private final SynchronousQueue<Object[]> fromCoroutine =
      new SynchronousQueue<>();
  private final V6Callable body;
  private final V6Value thisArg;
  private final V6Value[] args;
  private Thread thread;
  private boolean started = false;
  private boolean finished = false;

  public V6Generator(V6Callable body, V6Value thisArg, V6Value[] args) {
    this.body = body;
    this.thisArg = thisArg;
    this.args = args;
  }

  private void ensureStarted() {
    if (started)
      return;
    started = true;
    thread = new Thread(() -> {
      CURRENT.set(this);
      try {
        toCoroutine.take();
        V6Value result;
        try {
          result = body.call(thisArg, args);
        } catch (V6GeneratorReturn gr) {
          result = gr.value;
        }
        fromCoroutine.put(new Object[] {DONE, result});
      } catch (InterruptedException ie) {
        // thread interrupted during shutdown; nothing to report
      } catch (Throwable t) {
        try {
          fromCoroutine.put(new Object[] {ERROR, t});
        } catch (InterruptedException ignored) {
        }
      }
    });
    thread.setDaemon(true);
    thread.start();
  }

  public V6Value doYield(V6Value v) {
    try {
      fromCoroutine.put(new Object[] {YIELD, v});
      Object[] msg = toCoroutine.take();
      int kind = (Integer)msg[0];
      if (kind == THROW)
        throw new V6Throw((V6Value)msg[1]);
      if (kind == RETURN)
        throw new V6GeneratorReturn((V6Value)msg[1]);
      return (V6Value)msg[1];
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }
  }

  public static V6Value currentYield(V6Value v) {
    V6Generator g = CURRENT.get();
    if (g == null)
      throw new RuntimeException("yield used outside a generator");
    return g.doYield(v);
  }

  public V6Value doAwait(V6Value v) {
    try {
      fromCoroutine.put(new Object[] {AWAIT_YIELD, v});
      Object[] msg = toCoroutine.take();
      int kind = (Integer)msg[0];
      if (kind == THROW)
        throw new V6Throw((V6Value)msg[1]);
      if (kind == RETURN)
        throw new V6GeneratorReturn((V6Value)msg[1]);
      return (V6Value)msg[1];
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }
  }

  public static V6Value currentAwait(V6Value v) {
    V6Generator g = CURRENT.get();
    if (g == null)
      throw new RuntimeException("await used outside an async function");
    return g.doAwait(v);
  }

  public Object[] rawStep(int action, V6Value payload) {
    if (finished)
      return new Object[] {DONE, new V6Value(V6Value.TAG_UNDEF, 0, null)};
    ensureStarted();
    try {
      toCoroutine.put(new Object[] {action, payload});
      Object[] res = fromCoroutine.take();
      int kind = (Integer)res[0];
      if (kind == YIELD || kind == AWAIT_YIELD)
        return res;
      finished = true;
      return res;
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }
  }

  private V6Value iterResult(V6Value value, boolean done) {
    V6Object o = new V6Object();
    o.set("value", value);
    o.set("done", new V6Value(V6Value.TAG_BOOL, done ? 1 : 0, null));
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  public V6Value next(V6Value sent) {
    if (finished)
      return iterResult(new V6Value(V6Value.TAG_UNDEF, 0, null), true);
    ensureStarted();
    try {
      toCoroutine.put(new Object[] {RESUME, sent});
      Object[] res = fromCoroutine.take();
      int kind = (Integer)res[0];
      if (kind == YIELD)
        return iterResult((V6Value)res[1], false);
      finished = true;
      if (kind == ERROR) {
        Throwable t = (Throwable)res[1];
        if (t instanceof RuntimeException)
          throw (RuntimeException)t;
        throw new RuntimeException(t);
      }
      return iterResult((V6Value)res[1], true);
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }
  }

  public V6Value returnValue(V6Value v) {
    if (finished || !started) {
      finished = true;
      return iterResult(v, true);
    }
    try {
      toCoroutine.put(new Object[] {RETURN, v});
      Object[] res = fromCoroutine.take();
      int kind = (Integer)res[0];
      finished = true;
      if (kind == ERROR) {
        Throwable t = (Throwable)res[1];
        if (t instanceof RuntimeException)
          throw (RuntimeException)t;
        throw new RuntimeException(t);
      }
      return iterResult((V6Value)res[1], true);
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }
  }

  public V6Value throwInto(V6Value err) {
    if (finished || !started) {
      finished = true;
      throw new V6Throw(err);
    }
    try {
      toCoroutine.put(new Object[] {THROW, err});
      Object[] res = fromCoroutine.take();
      int kind = (Integer)res[0];
      if (kind == YIELD)
        return iterResult((V6Value)res[1], false);
      finished = true;
      if (kind == ERROR) {
        Throwable t = (Throwable)res[1];
        if (t instanceof RuntimeException)
          throw (RuntimeException)t;
        throw new RuntimeException(t);
      }
      return iterResult((V6Value)res[1], true);
    } catch (InterruptedException e) {
      throw new RuntimeException(e);
    }
  }
}

public final class V6AsyncGenerator extends V6Object {
  private final V6Generator gen;

  public V6AsyncGenerator(V6Callable body, V6Value thisArg, V6Value[] args) {
    this.gen = new V6Generator(body, thisArg, args);
  }

  private V6Value iterResult(V6Value value, boolean done) {
    V6Object o = new V6Object();
    o.set("value", value);
    o.set("done", new V6Value(V6Value.TAG_BOOL, done ? 1 : 0, null));
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private void drive(int action, V6Value payload, V6Promise result) {
    Object[] res;
    try {
      res = gen.rawStep(action, payload);
    } catch (V6Throw e) {
      result.reject(e.value);
      return;
    } catch (RuntimeException e) {
      result.reject(
          new V6Value(V6Value.TAG_STR, 0, String.valueOf(e.getMessage())));
      return;
    }
    int kind = (Integer)res[0];
    if (kind == V6Generator.YIELD) {
      result.resolve(iterResult((V6Value)res[1], false));
      return;
    }
    if (kind == V6Generator.DONE) {
      result.resolve(iterResult((V6Value)res[1], true));
      return;
    }
    if (kind == V6Generator.ERROR) {
      Throwable t = (Throwable)res[1];
      if (t instanceof V6Throw)
        result.reject(((V6Throw)t).value);
      else
        result.reject(
            new V6Value(V6Value.TAG_STR, 0, String.valueOf(t.getMessage())));
      return;
    }
    V6Value awaitedValue = (V6Value)res[1];
    V6Promise awaited = V6Promise.resolved(awaitedValue);
    awaited.addCallbacks(v
                         -> drive(V6Generator.RESUME, v, result),
                         err -> drive(V6Generator.THROW, err, result));
  }

  private V6Value driveAsPromise(int action, V6Value payload) {
    V6Promise result = new V6Promise();
    drive(action, payload, result);
    return new V6Value(V6Value.TAG_OBJ, 0, result);
  }

  public V6Value next(V6Value sent) {
    return driveAsPromise(V6Generator.RESUME, sent);
  }

  public V6Value throwInto(V6Value err) {
    return driveAsPromise(V6Generator.THROW, err);
  }

  public V6Value returnValue(V6Value v) {
    return driveAsPromise(V6Generator.RETURN, v);
  }
}

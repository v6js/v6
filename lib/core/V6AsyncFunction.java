public final class V6AsyncFunction implements V6Callable {
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private final V6Callable underlying;

  public V6AsyncFunction(V6Callable underlying) {
    this.underlying = underlying;
  }

  @Override
  public V6Value call(V6Value thisArg, V6Value[] args) {
    V6Generator gen = new V6Generator(underlying, thisArg, args);
    V6Promise result = new V6Promise();
    step(gen, result, UNDEF, false);
    return new V6Value(V6Value.TAG_OBJ, 0, result);
  }

  private static void step(V6Generator gen, V6Promise result, V6Value sent,
                           boolean isThrow) {
    V6Value stepResult;
    try {
      stepResult = isThrow ? gen.throwInto(sent) : gen.next(sent);
    } catch (V6Throw e) {
      result.reject(e.value);
      return;
    } catch (RuntimeException e) {
      result.reject(new V6Value(V6Value.TAG_STR, 0, String.valueOf(e.getMessage())));
      return;
    }
    V6Object obj = (V6Object)stepResult.ref();
    boolean done = obj.get("done").truthy();
    V6Value value = obj.get("value");
    if (done) {
      result.resolve(value);
      return;
    }
    V6Promise awaited = V6Promise.resolved(value);
    awaited.addCallbacks(v -> step(gen, result, v, false),
                         err -> step(gen, result, err, true));
  }
}

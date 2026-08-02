public final class V6AbortControllerConstructor
    extends V6Object implements V6NativeConstructor {
  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object self = new V6Object();
    V6AbortSignalObject signal = V6AbortSignalConstructor.newSignal();
    self.set("signal", objValue(signal));
    self.set("abort", fn((t, a) -> {
               V6Value reason =
                   a.length > 0 && !a[0].isUndefined()
                       ? a[0]
                       : str("AbortError: signal is aborted without reason");
               V6AbortSignalConstructor.fireAbort(signal, reason);
               return UNDEF;
             }));
    return objValue(self);
  }
}

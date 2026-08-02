public final class V6CountQueuingStrategyConstructor
    extends V6Object implements V6NativeConstructor {
  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object self = new V6Object();
    double hwm = 1;
    V6Value optsVal = V6Value.argAt(args, 0);
    if (optsVal.tag() == V6Value.TAG_OBJ && optsVal.ref() instanceof V6Object)
      hwm = ((V6Object)optsVal.ref()).get("highWaterMark").toNumber();
    self.set("highWaterMark", num(hwm));
    self.set("size", fn((t, a) -> num(1)));
    return objValue(self);
  }
}

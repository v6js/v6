public final class V6Tty {
  private V6Tty() {
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value boolValue(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  public static V6Object build() {
    V6Object o = new V6Object();
    o.set("isatty", fn((thisArg, args) -> boolValue(System.console() != null)));
    o.set("ReadStream", fn((thisArg, args) -> {
            throw new V6Throw(
                new V6Value(V6Value.TAG_STR, 0,
                            "TypeError: tty.ReadStream is not supported"));
          }));
    o.set("WriteStream", fn((thisArg, args) -> {
            throw new V6Throw(
                new V6Value(V6Value.TAG_STR, 0,
                            "TypeError: tty.WriteStream is not supported"));
          }));
    return o;
  }

  public static final V6Value MODULE = new V6Value(V6Value.TAG_OBJ, 0, build());
}

public final class V6Inspector {
  private V6Inspector() {
  }

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

  private static final String MSG =
      "not supported (no V8 Inspector protocol implementation on the JVM)";

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("open", fn((thisArg, args) -> {
            throw new V6Throw(str("inspector.open is " + MSG));
          }));
    o.set("close", fn((thisArg, args) -> UNDEF));
    o.set("url", fn((thisArg, args) -> UNDEF));
    o.set("waitForDebugger", fn((thisArg, args) -> {
            throw new V6Throw(str("inspector.waitForDebugger is " + MSG));
          }));

    o.set("Session", objValue(new V6UnsupportedConstructor(
                         "inspector.Session is " + MSG)));

    return o;
  }
}

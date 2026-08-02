public final class V6PerfHooks {
  private V6PerfHooks() {
  }

  private static final long START_NANO = System.nanoTime();
  private static final long START_EPOCH_MS = System.currentTimeMillis();

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  public static V6Object build() {
    V6Object o = new V6Object();
    V6Object performance = new V6Object();
    performance.set(
        "now",
        fn((t, a) -> num((System.nanoTime() - START_NANO) / 1_000_000.0)));
    performance.set("timeOrigin", num(START_EPOCH_MS));
    o.set("performance", objValue(performance));
    return o;
  }
}

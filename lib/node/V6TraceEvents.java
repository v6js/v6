public final class V6TraceEvents {
  private V6TraceEvents() {
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("createTracing", fn((thisArg, args) -> {
            throw new V6Throw(str("trace_events.createTracing is not "
                                  + "supported (no V8 trace event backend "
                                  + "on the JVM)"));
          }));

    o.set("getEnabledCategories", fn((thisArg, args) -> UNDEF));

    return o;
  }
}

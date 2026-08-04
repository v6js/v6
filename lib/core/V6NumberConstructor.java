public final class V6NumberConstructor
    extends V6Object implements V6NativeConstructor {
  public V6NumberConstructor() {
    set("isInteger", fn((thisArg, args) -> {
          V6Value v = V6Value.argAt(args, 0);
          if (v.tag() != V6Value.TAG_NUM)
            return boolValue(false);
          double n = v.toNumber();
          return boolValue(!Double.isNaN(n) && !Double.isInfinite(n) &&
                           n == Math.floor(n));
        }));
    set("isFinite", fn((thisArg, args) -> {
          V6Value v = V6Value.argAt(args, 0);
          if (v.tag() != V6Value.TAG_NUM)
            return boolValue(false);
          double n = v.toNumber();
          return boolValue(!Double.isNaN(n) && !Double.isInfinite(n));
        }));
    set("isNaN", fn((thisArg, args) -> {
          V6Value v = V6Value.argAt(args, 0);
          return boolValue(v.tag() == V6Value.TAG_NUM &&
                           Double.isNaN(v.toNumber()));
        }));
    set("isSafeInteger", fn((thisArg, args) -> {
          V6Value v = V6Value.argAt(args, 0);
          if (v.tag() != V6Value.TAG_NUM)
            return boolValue(false);
          double n = v.toNumber();
          return boolValue(!Double.isNaN(n) && !Double.isInfinite(n) &&
                           n == Math.floor(n) &&
                           Math.abs(n) <= 9007199254740991.0);
        }));
    set("parseFloat", V6Builtins.PARSE_FLOAT);
    set("parseInt", V6Builtins.PARSE_INT);
    set("EPSILON", new V6Value(V6Value.TAG_NUM, Math.ulp(1.0), null));
    set("MAX_SAFE_INTEGER",
        new V6Value(V6Value.TAG_NUM, 9007199254740991.0, null));
    set("MIN_SAFE_INTEGER",
        new V6Value(V6Value.TAG_NUM, -9007199254740991.0, null));
    set("MAX_VALUE", new V6Value(V6Value.TAG_NUM, Double.MAX_VALUE, null));
    set("MIN_VALUE", new V6Value(V6Value.TAG_NUM, Double.MIN_VALUE, null));
    set("POSITIVE_INFINITY",
        new V6Value(V6Value.TAG_NUM, Double.POSITIVE_INFINITY, null));
    set("NEGATIVE_INFINITY",
        new V6Value(V6Value.TAG_NUM, Double.NEGATIVE_INFINITY, null));
    set("NaN", new V6Value(V6Value.TAG_NUM, Double.NaN, null));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    if (args.length == 0)
      return new V6Value(V6Value.TAG_NUM, 0, null);
    return new V6Value(V6Value.TAG_NUM, V6Value.argAt(args, 0).toNumber(),
                       null);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value boolValue(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }
}

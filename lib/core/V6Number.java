public final class V6Number extends V6Object {
  public static final V6Number PROTOTYPE = new V6Number();

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private V6Number() {
    set("toFixed", fn((thisArg, args) -> {
          double n = thisArg.toNumber();
          int digits = args.length > 0 ? (int)args[0].toNumber() : 0;
          java.math.BigDecimal bd = new java.math.BigDecimal(n).setScale(
              digits, java.math.RoundingMode.HALF_UP);
          return str(bd.toPlainString());
        }));
    set("toPrecision", fn((thisArg, args) -> {
          double n = thisArg.toNumber();
          if (args.length == 0 || args[0].isUndefined())
            return str(thisArg.toString());
          int precision = (int)args[0].toNumber();
          java.math.BigDecimal bd = new java.math.BigDecimal(n).round(
              new java.math.MathContext(precision));
          return str(bd.toPlainString());
        }));
    set("toString", fn((thisArg, args) -> {
          double n = thisArg.toNumber();
          if (args.length > 0 && !args[0].isUndefined()) {
            int radix = (int)args[0].toNumber();
            if (radix != 10) {
              long asLong = (long)n;
              return str(Long.toString(asLong, radix));
            }
          }
          return str(thisArg.toString());
        }));
    set("valueOf",
        fn((thisArg,
            args) -> new V6Value(V6Value.TAG_NUM, thisArg.toNumber(), null)));
  }
}

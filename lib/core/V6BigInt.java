public final class V6BigInt extends V6Object {
  public static final V6BigInt PROTOTYPE = new V6BigInt();

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private V6BigInt() {
    set("toString", fn((thisArg, args) -> {
          java.math.BigInteger n = thisArg.asBigInt();
          int radix = args.length > 0 && !args[0].isUndefined()
                          ? (int)args[0].toNumber()
                          : 10;
          return str(n.toString(radix));
        }));
    set("toLocaleString",
        fn((thisArg, args) -> str(thisArg.asBigInt().toString())));
    set("valueOf", fn((thisArg, args) -> V6Value.bigint(thisArg.asBigInt())));
  }
}

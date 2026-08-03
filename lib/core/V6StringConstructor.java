public final class V6StringConstructor
    extends V6Object implements V6NativeConstructor {
  public V6StringConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, V6String.PROTOTYPE));
    set("fromCharCode",
        new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(thisArg, args) -> {
          StringBuilder sb = new StringBuilder();
          for (V6Value v : args)
            sb.append((char)(int)v.toNumber());
          return new V6Value(V6Value.TAG_STR, 0, sb.toString());
        }));
    set("fromCodePoint",
        new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(thisArg, args) -> {
          StringBuilder sb = new StringBuilder();
          for (V6Value v : args)
            sb.appendCodePoint((int)v.toNumber());
          return new V6Value(V6Value.TAG_STR, 0, sb.toString());
        }));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    String s = args.length > 0 ? args[0].toString() : "";
    return new V6Value(V6Value.TAG_STR, 0, s);
  }
}

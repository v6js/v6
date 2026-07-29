public final class V6SymbolFunction extends V6Object implements V6Callable {
  @Override
  public V6Value call(V6Value thisArg, V6Value[] args) {
    String desc =
        args.length > 0 && !args[0].isUndefined() ? args[0].toString() : null;
    return new V6Value(V6Value.TAG_OBJ, 0, new V6Symbol(desc));
  }
}

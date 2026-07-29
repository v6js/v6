public final class V6AsyncGeneratorFunction implements V6Callable {
  private final V6Callable underlying;

  public V6AsyncGeneratorFunction(V6Callable underlying) {
    this.underlying = underlying;
  }

  @Override
  public V6Value call(V6Value thisArg, V6Value[] args) {
    V6AsyncGenerator gen = new V6AsyncGenerator(underlying, thisArg, args);
    gen.setProto(V6Builtins.ASYNC_GENERATOR_PROTOTYPE);
    return new V6Value(V6Value.TAG_OBJ, 0, gen);
  }
}

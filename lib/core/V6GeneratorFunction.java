public final class V6GeneratorFunction implements V6Callable {
  private final V6Callable underlying;

  public V6GeneratorFunction(V6Callable underlying) {
    this.underlying = underlying;
  }

  @Override
  public V6Value call(V6Value thisArg, V6Value[] args) {
    V6Generator gen = new V6Generator(underlying, thisArg, args);
    gen.setProto(V6Builtins.GENERATOR_PROTOTYPE);
    return new V6Value(V6Value.TAG_OBJ, 0, gen);
  }
}

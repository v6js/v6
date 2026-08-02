public final class V6NativeFunctionObject
    extends V6Object implements V6Callable {
  private final V6Callable delegate;

  public V6NativeFunctionObject(V6Callable delegate) {
    this.delegate = delegate;
  }

  @Override
  public V6Value call(V6Value thisArg, V6Value[] args) {
    return delegate.call(thisArg, args);
  }
}

public final class V6UnsupportedConstructor
    extends V6Object implements V6NativeConstructor {
  private final String message;

  public V6UnsupportedConstructor(String message) {
    this.message = message;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    throw new V6Throw(new V6Value(V6Value.TAG_STR, 0, message));
  }
}

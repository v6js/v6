public final class V6BooleanConstructor
    extends V6Object implements V6NativeConstructor {
  public V6BooleanConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, V6Boolean.PROTOTYPE));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    boolean b = args.length > 0 && args[0].truthy();
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }
}

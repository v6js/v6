public final class V6FunctionConstructor
    extends V6Object implements V6NativeConstructor {
  public V6FunctionConstructor() {
    setProto(V6Closure.FUNCTION_PROTOTYPE);
    set("prototype",
        new V6Value(V6Value.TAG_OBJ, 0, V6Closure.FUNCTION_PROTOTYPE));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    throw new RuntimeException("Function constructor is not supported");
  }
}

public interface V6NativeConstructor {
  V6Value construct(V6Value[] args);

  default V6Object prototypeObject() {
    return null;
  }

  default V6Object allocate() {
    return null;
  }

  default void initInstance(V6Object instance, V6Value[] args) {
  }
}

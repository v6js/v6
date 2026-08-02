public interface V6NativeConstructor {
  V6Value construct(V6Value[] args);
  default V6Object prototypeObject() { return null; }
}

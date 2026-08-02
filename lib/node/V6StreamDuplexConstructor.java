public final class V6StreamDuplexConstructor extends V6Object
    implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6StreamDuplexConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  @Override
  public V6Object allocate() {
    V6EventEmitterObject e = new V6EventEmitterObject();
    e.setProto(PROTOTYPE);
    return e;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    return new V6Value(V6Value.TAG_OBJ, 0, allocate());
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventEmitterConstructor.PROTOTYPE);
    V6StreamMethods.installReadable(o);
    V6StreamMethods.installWritable(o);
    return o;
  }
}

public final class V6CustomEventConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6CustomEventConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  @Override
  public V6Object allocate() {
    return new V6CustomEventObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6CustomEventObject e = new V6CustomEventObject();
    e.setProto(PROTOTYPE);
    initInstance(e, args);
    return new V6Value(V6Value.TAG_OBJ, 0, e);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6CustomEventObject e = (V6CustomEventObject)instance;
    e.type = V6Value.argAt(args, 0).toString();
    V6Value initVal = V6Value.argAt(args, 1);
    V6EventConstructor.applyInit(e, initVal);
    if (initVal.tag() == V6Value.TAG_OBJ && initVal.ref() instanceof V6Object) {
      V6Value detail = ((V6Object)initVal.ref()).get("detail");
      if (!detail.isUndefined())
        e.detail = detail;
    }
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventConstructor.PROTOTYPE);
    o.defineGetter("detail", (t, a) -> ((V6CustomEventObject)t.ref()).detail);
    return o;
  }
}

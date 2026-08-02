public final class V6MessageEventConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6MessageEventConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  @Override
  public V6Object allocate() {
    return new V6MessageEventObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6MessageEventObject e = new V6MessageEventObject();
    e.setProto(PROTOTYPE);
    initInstance(e, args);
    return objValue(e);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6MessageEventObject e = (V6MessageEventObject)instance;
    e.type = V6Value.argAt(args, 0).toString();
    V6Value initVal = V6Value.argAt(args, 1);
    V6EventConstructor.applyInit(e, initVal);
    if (initVal.tag() == V6Value.TAG_OBJ && initVal.ref() instanceof V6Object) {
      V6Value data = ((V6Object)initVal.ref()).get("data");
      if (!data.isUndefined())
        e.data = data;
    }
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventConstructor.PROTOTYPE);
    o.defineGetter("data", (t, a) -> ((V6MessageEventObject)t.ref()).data);
    return o;
  }
}

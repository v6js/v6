public final class V6FileConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6FileConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  @Override
  public V6Object allocate() {
    return new V6FileObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6FileObject f = new V6FileObject();
    f.setProto(PROTOTYPE);
    initInstance(f, args);
    return objValue(f);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6FileObject f = (V6FileObject)instance;
    f.data = V6BlobConstructor.concatParts(V6Value.argAt(args, 0));
    f.name = V6Value.argAt(args, 1).toString();
    f.type = V6BlobConstructor.extractType(V6Value.argAt(args, 2));
    double lastModified = System.currentTimeMillis();
    V6Value optionsVal = V6Value.argAt(args, 2);
    if (optionsVal.tag() == V6Value.TAG_OBJ && optionsVal.ref() instanceof
                                                   V6Object) {
      V6Value lm = ((V6Object)optionsVal.ref()).get("lastModified");
      if (!lm.isUndefined())
        lastModified = lm.toNumber();
    }
    f.lastModified = lastModified;
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6FileObject self(V6Value t) {
    return (V6FileObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6BlobConstructor.PROTOTYPE);
    o.defineGetter("name", (t, a) -> str(self(t).name));
    o.defineGetter("lastModified", (t, a) -> num(self(t).lastModified));
    return o;
  }
}

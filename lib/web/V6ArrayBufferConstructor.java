public final class V6ArrayBufferConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6ArrayBufferConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
    set("isView", fn((thisArg, args) -> {
          V6Value v = V6Value.argAt(args, 0);
          return bool(v.tag() == V6Value.TAG_OBJ && v.ref() instanceof
                                                        V6Buffer);
        }));
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  public static V6Value wrap(byte[] bytes) {
    V6ArrayBufferObject o = new V6ArrayBufferObject(bytes);
    o.setProto(PROTOTYPE);
    return objValue(o);
  }

  @Override
  public V6Value construct(V6Value[] args) {
    int len = (int)V6Value.argAt(args, 0).toNumber();
    return wrap(new byte[Math.max(0, len)]);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6ArrayBufferObject self(V6Value t) {
    return (V6ArrayBufferObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.defineGetter("byteLength", (t, a) -> num(self(t).data.length));
    o.set("slice", fn((t, a) -> {
            byte[] data = self(t).data;
            int start = a.length > 0
                            ? normalizeIndex((int)a[0].toNumber(), data.length)
                            : 0;
            int end = a.length > 1 && !a[1].isUndefined()
                          ? normalizeIndex((int)a[1].toNumber(), data.length)
                          : data.length;
            if (end < start)
              end = start;
            byte[] out = java.util.Arrays.copyOfRange(data, start, end);
            return wrap(out);
          }));
    return o;
  }

  private static int normalizeIndex(int idx, int len) {
    if (idx < 0)
      idx = Math.max(0, len + idx);
    return Math.min(idx, len);
  }
}

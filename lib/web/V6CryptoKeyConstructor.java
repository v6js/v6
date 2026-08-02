public final class V6CryptoKeyConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6CryptoKeyConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  public static V6CryptoKeyObject newKey() {
    V6CryptoKeyObject k = new V6CryptoKeyObject();
    k.setProto(PROTOTYPE);
    return k;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    throw new V6Throw(str("TypeError: Illegal constructor"));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6CryptoKeyObject self(V6Value t) {
    return (V6CryptoKeyObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.defineGetter("type", (t, a) -> str(self(t).type));
    o.defineGetter("extractable", (t, a) -> bool(self(t).extractable));
    o.defineGetter("algorithm", (t, a) -> objValue(self(t).algorithmObj));
    o.defineGetter("usages", (t, a) -> objValue(self(t).usages));
    return o;
  }
}

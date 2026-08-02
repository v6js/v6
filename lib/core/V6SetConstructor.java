public final class V6SetConstructor extends V6Object implements V6NativeConstructor {
  @Override
  public V6Value construct(V6Value[] args) {
    V6SetObject s = new V6SetObject();
    s.setProto(V6Builtins.SET_PROTOTYPE);
    if (args.length > 0 && args[0].tag() == V6Value.TAG_OBJ) {
      V6Object iterable = (V6Object)args[0].ref();
      int n = (int)iterable.get("length").num();
      for (int i = 0; i < n; i++) {
        V6Value v = iterable.get(Integer.toString(i));
        s.entries.put(V6MapObject.keyFor(v), v);
      }
    }
    return new V6Value(V6Value.TAG_OBJ, 0, s);
  }

  @Override
  public V6Object prototypeObject() {
    return V6Builtins.SET_PROTOTYPE;
  }
}

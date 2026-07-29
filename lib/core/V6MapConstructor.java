public final class V6MapConstructor extends V6Object implements V6NativeConstructor {
  @Override
  public V6Value construct(V6Value[] args) {
    V6MapObject m = new V6MapObject();
    m.setProto(V6Builtins.MAP_PROTOTYPE);
    if (args.length > 0 && args[0].tag() == V6Value.TAG_OBJ) {
      V6Object iterable = (V6Object)args[0].ref();
      int n = (int)iterable.get("length").num();
      for (int i = 0; i < n; i++) {
        Object pairRef = iterable.get(Integer.toString(i)).ref();
        if (pairRef instanceof V6Object) {
          V6Object pair = (V6Object)pairRef;
          m.entries.put(V6MapObject.keyFor(pair.get("0")), pair.get("1"));
        }
      }
    }
    return new V6Value(V6Value.TAG_OBJ, 0, m);
  }
}

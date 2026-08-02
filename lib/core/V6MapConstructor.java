public final class V6MapConstructor
    extends V6Object implements V6NativeConstructor {
  public V6MapConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, V6Builtins.MAP_PROTOTYPE));
    V6Builtins.MAP_PROTOTYPE.nativeCtor = this;
  }

  @Override
  public V6Object allocate() {
    return new V6MapObject();
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6MapObject m = (V6MapObject)instance;
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
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6MapObject m = new V6MapObject();
    m.setProto(V6Builtins.MAP_PROTOTYPE);
    initInstance(m, args);
    return new V6Value(V6Value.TAG_OBJ, 0, m);
  }

  @Override
  public V6Object prototypeObject() {
    return V6Builtins.MAP_PROTOTYPE;
  }
}

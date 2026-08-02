public final class V6HeadersConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6HeadersConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);

  public static V6HeadersObject newHeaders(V6Value init) {
    V6HeadersObject h = new V6HeadersObject();
    h.setProto(PROTOTYPE);
    if (init.tag() == V6Value.TAG_OBJ && init.ref() instanceof
                                             V6HeadersObject) {
      h.map.putAll(((V6HeadersObject)init.ref()).map);
    } else if (init.tag() == V6Value.TAG_OBJ && init.ref() instanceof V6Array) {
      V6Array arr = (V6Array)init.ref();
      for (int i = 0; i < arr.elemCount; i++) {
        V6Value entry = arr.elements[i];
        if (entry.tag() == V6Value.TAG_OBJ && entry.ref() instanceof V6Array) {
          V6Array pair = (V6Array)entry.ref();
          if (pair.elemCount >= 2)
            h.map.put(pair.elements[0].toString().toLowerCase(),
                      pair.elements[1].toString());
        }
      }
    } else if (init.tag() == V6Value.TAG_OBJ && init.ref() instanceof
                                                    V6Object) {
      V6Object obj = (V6Object)init.ref();
      V6Array keys = obj.enumKeys();
      for (int i = 0; i < keys.elemCount; i++) {
        String k = keys.elements[i].toString();
        h.map.put(k.toLowerCase(), obj.get(k).toString());
      }
    }
    return h;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    return objValue(newHeaders(V6Value.argAt(args, 0)));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6HeadersObject self(V6Value t) {
    return (V6HeadersObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.set("append", fn((t, a) -> {
            V6HeadersObject h = self(t);
            String name = V6Value.argAt(a, 0).toString().toLowerCase();
            String value = V6Value.argAt(a, 1).toString();
            String existing = h.map.get(name);
            h.map.put(name, existing == null ? value : existing + ", " + value);
            return UNDEF;
          }));

    o.set("set", fn((t, a) -> {
            self(t).map.put(V6Value.argAt(a, 0).toString().toLowerCase(),
                            V6Value.argAt(a, 1).toString());
            return UNDEF;
          }));

    o.set("get", fn((t, a) -> {
            String v =
                self(t).map.get(V6Value.argAt(a, 0).toString().toLowerCase());
            return v == null ? NUL : str(v);
          }));

    o.set("has", fn((t, a)
                        -> new V6Value(
                            V6Value.TAG_BOOL,
                            self(t).map.containsKey(
                                V6Value.argAt(a, 0).toString().toLowerCase())
                                ? 1
                                : 0,
                            null)));

    o.set("delete", fn((t, a) -> {
            self(t).map.remove(V6Value.argAt(a, 0).toString().toLowerCase());
            return UNDEF;
          }));

    o.set("forEach", fn((t, a) -> {
            V6HeadersObject h = self(t);
            V6Callable cb = V6Value.argAt(a, 0).asCallable();
            for (java.util.Map.Entry<String, String> e : h.map.entrySet())
              cb.call(UNDEF,
                      new V6Value[] {str(e.getValue()), str(e.getKey()), t});
            return UNDEF;
          }));

    o.set("keys", fn((t, a) -> {
            V6Array result = new V6Array();
            for (String k : self(t).map.keySet())
              result.push(str(k));
            return objValue(result);
          }));

    o.set("values", fn((t, a) -> {
            V6Array result = new V6Array();
            for (String v : self(t).map.values())
              result.push(str(v));
            return objValue(result);
          }));

    o.set("entries", fn((t, a) -> {
            V6Array result = new V6Array();
            for (java.util.Map.Entry<String, String> e :
                 self(t).map.entrySet()) {
              V6Array pair = new V6Array();
              pair.push(str(e.getKey()));
              pair.push(str(e.getValue()));
              result.push(objValue(pair));
            }
            return objValue(result);
          }));

    return o;
  }
}

public final class V6FormDataConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6FormDataConstructor() {
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

  @Override
  public V6Object allocate() {
    return new V6FormDataObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6FormDataObject o = new V6FormDataObject();
    o.setProto(PROTOTYPE);
    return objValue(o);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6FormDataObject self(V6Value t) {
    return (V6FormDataObject)t.ref();
  }

  private static V6Value normalizeValue(V6Value value, V6Value filenameVal) {
    if (value.tag() == V6Value.TAG_OBJ && value.ref() instanceof V6BlobObject &&
        !(value.ref() instanceof V6FileObject) && !filenameVal.isUndefined()) {
      V6BlobObject b = (V6BlobObject)value.ref();
      V6FileObject f = new V6FileObject();
      f.setProto(V6FileConstructor.PROTOTYPE);
      f.data = b.data;
      f.type = b.type;
      f.name = filenameVal.toString();
      f.lastModified = System.currentTimeMillis();
      return objValue(f);
    }
    if (value.tag() == V6Value.TAG_STR)
      return value;
    if (value.tag() == V6Value.TAG_OBJ &&
        (value.ref() instanceof V6BlobObject || value.ref() instanceof
                                                    V6FileObject))
      return value;
    return str(value.toString());
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.set("append", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            fd.keys.add(V6Value.argAt(a, 0).toString());
            fd.values.add(
                normalizeValue(V6Value.argAt(a, 1), V6Value.argAt(a, 2)));
            return UNDEF;
          }));

    o.set("set", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            String name = V6Value.argAt(a, 0).toString();
            V6Value normalized =
                normalizeValue(V6Value.argAt(a, 1), V6Value.argAt(a, 2));
            boolean replaced = false;
            for (int i = fd.keys.size() - 1; i >= 0; i--) {
              if (fd.keys.get(i).equals(name)) {
                if (!replaced) {
                  fd.values.set(i, normalized);
                  replaced = true;
                } else {
                  fd.keys.remove(i);
                  fd.values.remove(i);
                }
              }
            }
            if (!replaced) {
              fd.keys.add(name);
              fd.values.add(normalized);
            }
            return UNDEF;
          }));

    o.set("get", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            String name = V6Value.argAt(a, 0).toString();
            for (int i = 0; i < fd.keys.size(); i++)
              if (fd.keys.get(i).equals(name))
                return fd.values.get(i);
            return NUL;
          }));

    o.set("getAll", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            String name = V6Value.argAt(a, 0).toString();
            V6Array result = new V6Array();
            for (int i = 0; i < fd.keys.size(); i++)
              if (fd.keys.get(i).equals(name))
                result.push(fd.values.get(i));
            return objValue(result);
          }));

    o.set("has", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            String name = V6Value.argAt(a, 0).toString();
            for (String k : fd.keys)
              if (k.equals(name))
                return new V6Value(V6Value.TAG_BOOL, 1, null);
            return new V6Value(V6Value.TAG_BOOL, 0, null);
          }));

    o.set("delete", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            String name = V6Value.argAt(a, 0).toString();
            for (int i = fd.keys.size() - 1; i >= 0; i--) {
              if (fd.keys.get(i).equals(name)) {
                fd.keys.remove(i);
                fd.values.remove(i);
              }
            }
            return UNDEF;
          }));

    o.set("forEach", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            V6Callable cb = V6Value.argAt(a, 0).asCallable();
            for (int i = 0; i < fd.keys.size(); i++)
              cb.call(UNDEF,
                      new V6Value[] {fd.values.get(i), str(fd.keys.get(i)), t});
            return UNDEF;
          }));

    o.set("keys", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            V6Array result = new V6Array();
            for (String k : fd.keys)
              result.push(str(k));
            return objValue(result);
          }));

    o.set("values", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            V6Array result = new V6Array();
            for (V6Value v : fd.values)
              result.push(v);
            return objValue(result);
          }));

    o.set("entries", fn((t, a) -> {
            V6FormDataObject fd = self(t);
            V6Array result = new V6Array();
            for (int i = 0; i < fd.keys.size(); i++) {
              V6Array pair = new V6Array();
              pair.push(str(fd.keys.get(i)));
              pair.push(fd.values.get(i));
              result.push(objValue(pair));
            }
            return objValue(result);
          }));

    return o;
  }
}

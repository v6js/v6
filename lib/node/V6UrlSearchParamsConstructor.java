import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.net.URLEncoder;

public final class V6UrlSearchParamsConstructor extends V6Object
    implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6UrlSearchParamsConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
  }

  static String encode(String s) {
    try {
      return URLEncoder.encode(s, "UTF-8");
    } catch (UnsupportedEncodingException e) {
      return s;
    }
  }

  static String decode(String s) {
    try {
      return URLDecoder.decode(s, "UTF-8");
    } catch (Exception e) {
      return s;
    }
  }

  static V6UrlSearchParamsObject parseInto(String query) {
    V6UrlSearchParamsObject o = new V6UrlSearchParamsObject();
    o.setProto(PROTOTYPE);
    if (query != null && query.startsWith("?"))
      query = query.substring(1);
    if (query != null && !query.isEmpty()) {
      for (String pair : query.split("&", -1)) {
        if (pair.isEmpty())
          continue;
        int idx = pair.indexOf('=');
        String k = idx < 0 ? pair : pair.substring(0, idx);
        String v = idx < 0 ? "" : pair.substring(idx + 1);
        o.pairs.add(new String[] {decode(k), decode(v)});
      }
    }
    return o;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    V6UrlSearchParamsObject o;
    if (first.tag() == V6Value.TAG_STR) {
      o = parseInto(first.toString());
    } else if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof V6UrlSearchParamsObject) {
      o = new V6UrlSearchParamsObject();
      o.setProto(PROTOTYPE);
      for (String[] p : ((V6UrlSearchParamsObject)first.ref()).pairs)
        o.pairs.add(new String[] {p[0], p[1]});
    } else if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof V6Array) {
      o = new V6UrlSearchParamsObject();
      o.setProto(PROTOTYPE);
      V6Array arr = (V6Array)first.ref();
      int n = (int)arr.get("length").num();
      for (int i = 0; i < n; i++) {
        V6Value entry = arr.get(Integer.toString(i));
        if (entry.tag() == V6Value.TAG_OBJ) {
          V6Object pair = (V6Object)entry.ref();
          o.pairs.add(new String[] {pair.get("0").toString(), pair.get("1").toString()});
        }
      }
    } else if (first.tag() == V6Value.TAG_OBJ) {
      o = new V6UrlSearchParamsObject();
      o.setProto(PROTOTYPE);
      V6Object obj = (V6Object)first.ref();
      V6Array keys = obj.enumKeys();
      int n = (int)keys.get("length").num();
      for (int i = 0; i < n; i++) {
        String k = keys.get(Integer.toString(i)).toString();
        o.pairs.add(new String[] {k, obj.get(k).toString()});
      }
    } else {
      o = new V6UrlSearchParamsObject();
      o.setProto(PROTOTYPE);
    }
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6UrlSearchParamsObject self(V6Value t) {
    return (V6UrlSearchParamsObject)t.ref();
  }

  public static String stringify(V6UrlSearchParamsObject o) {
    StringBuilder sb = new StringBuilder();
    for (String[] p : o.pairs) {
      if (sb.length() > 0)
        sb.append('&');
      sb.append(encode(p[0])).append('=').append(encode(p[1]));
    }
    return sb.toString();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.set("append", fn((t, a) -> {
            V6UrlSearchParamsObject s = self(t);
            s.pairs.add(new String[] {V6Value.argAt(a, 0).toString(),
                                      V6Value.argAt(a, 1).toString()});
            s.notifyChange();
            return new V6Value(V6Value.TAG_UNDEF, 0, null);
          }));

    o.set("delete", fn((t, a) -> {
            V6UrlSearchParamsObject s = self(t);
            String key = V6Value.argAt(a, 0).toString();
            s.pairs.removeIf(p -> p[0].equals(key));
            s.notifyChange();
            return new V6Value(V6Value.TAG_UNDEF, 0, null);
          }));

    o.set("get", fn((t, a) -> {
            String key = V6Value.argAt(a, 0).toString();
            for (String[] p : self(t).pairs)
              if (p[0].equals(key))
                return str(p[1]);
            return new V6Value(V6Value.TAG_NULL, 0, null);
          }));

    o.set("getAll", fn((t, a) -> {
            String key = V6Value.argAt(a, 0).toString();
            V6Array result = new V6Array();
            for (String[] p : self(t).pairs)
              if (p[0].equals(key))
                result.push(str(p[1]));
            return objValue(result);
          }));

    o.set("has", fn((t, a) -> {
            String key = V6Value.argAt(a, 0).toString();
            for (String[] p : self(t).pairs)
              if (p[0].equals(key))
                return new V6Value(V6Value.TAG_BOOL, 1, null);
            return new V6Value(V6Value.TAG_BOOL, 0, null);
          }));

    o.set("set", fn((t, a) -> {
            V6UrlSearchParamsObject s = self(t);
            String key = V6Value.argAt(a, 0).toString();
            String val = V6Value.argAt(a, 1).toString();
            boolean placed = false;
            java.util.List<String[]> kept = new java.util.ArrayList<>();
            for (String[] p : s.pairs) {
              if (p[0].equals(key)) {
                if (!placed) {
                  kept.add(new String[] {key, val});
                  placed = true;
                }
              } else {
                kept.add(p);
              }
            }
            if (!placed)
              kept.add(new String[] {key, val});
            s.pairs.clear();
            s.pairs.addAll(kept);
            s.notifyChange();
            return new V6Value(V6Value.TAG_UNDEF, 0, null);
          }));

    o.set("sort", fn((t, a) -> {
            V6UrlSearchParamsObject s = self(t);
            s.pairs.sort((x, y) -> x[0].compareTo(y[0]));
            s.notifyChange();
            return new V6Value(V6Value.TAG_UNDEF, 0, null);
          }));

    o.set("forEach", fn((t, a) -> {
            V6Callable cb = V6Value.argAt(a, 0).asCallable();
            for (String[] p : self(t).pairs)
              cb.call(new V6Value(V6Value.TAG_UNDEF, 0, null),
                     new V6Value[] {str(p[1]), str(p[0]), t});
            return new V6Value(V6Value.TAG_UNDEF, 0, null);
          }));

    o.set("keys", fn((t, a) -> {
            V6Array result = new V6Array();
            for (String[] p : self(t).pairs)
              result.push(str(p[0]));
            return objValue(result);
          }));

    o.set("values", fn((t, a) -> {
            V6Array result = new V6Array();
            for (String[] p : self(t).pairs)
              result.push(str(p[1]));
            return objValue(result);
          }));

    o.set("entries", fn((t, a) -> {
            V6Array result = new V6Array();
            for (String[] p : self(t).pairs) {
              V6Array pair = new V6Array();
              pair.push(str(p[0]));
              pair.push(str(p[1]));
              result.push(objValue(pair));
            }
            return objValue(result);
          }));

    o.set("toString", fn((t, a) -> str(stringify(self(t)))));

    return o;
  }
}

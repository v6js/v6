import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.net.URLEncoder;

public final class V6QueryString {
  private V6QueryString() {
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

  private static String escapeInternal(String s) {
    try {
      return URLEncoder.encode(s, "UTF-8").replace("+", "%20");
    } catch (UnsupportedEncodingException e) {
      return s;
    }
  }

  private static String unescapeInternal(String s) {
    try {
      return URLDecoder.decode(s.replace("+", "%2B"), "UTF-8");
    } catch (Exception e) {
      return s;
    }
  }

  private static String decodeComponent(String s) {
    try {
      return URLDecoder.decode(s, "UTF-8");
    } catch (Exception e) {
      return s;
    }
  }

  private static String argStr(V6Value[] args, int idx, String def) {
    if (args.length <= idx || args[idx].isUndefined())
      return def;
    return args[idx].toString();
  }

  public static V6Object parseToObject(String s, String sep, String eq) {
    V6Object result = new V6Object();
    if (s.isEmpty())
      return result;
    for (String pair : s.split(java.util.regex.Pattern.quote(sep), -1)) {
      if (pair.isEmpty())
        continue;
      int idx = pair.indexOf(eq);
      String key, val;
      if (idx < 0) {
        key = pair;
        val = "";
      } else {
        key = pair.substring(0, idx);
        val = pair.substring(idx + eq.length());
      }
      key = decodeComponent(key);
      val = decodeComponent(val);
      V6Value existing = result.get(key);
      if (existing.isUndefined()) {
        result.set(key, str(val));
      } else if (existing.tag() == V6Value.TAG_OBJ && existing.ref() instanceof
                                                          V6Array) {
        ((V6Array)existing.ref()).push(str(val));
      } else {
        V6Array arr = new V6Array();
        arr.push(existing);
        arr.push(str(val));
        result.set(key, objValue(arr));
      }
    }
    return result;
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("escape",
          fn((thisArg,
              args) -> str(escapeInternal(V6Value.argAt(args, 0).toString()))));
    o.set(
        "unescape",
        fn((thisArg,
            args) -> str(unescapeInternal(V6Value.argAt(args, 0).toString()))));

    o.set("parse", fn((thisArg, args) -> {
            String s = V6Value.argAt(args, 0).toString();
            String sep = argStr(args, 1, "&");
            String eq = argStr(args, 2, "=");
            return objValue(parseToObject(s, sep, eq));
          }));
    o.set("decode", o.get("parse"));

    o.set("stringify", fn((thisArg, args) -> {
            V6Value objArg = V6Value.argAt(args, 0);
            String sep = argStr(args, 1, "&");
            String eq = argStr(args, 2, "=");
            if (objArg.tag() != V6Value.TAG_OBJ)
              return str("");
            V6Object obj = (V6Object)objArg.ref();
            V6Array keys = obj.enumKeys();
            int n = (int)keys.get("length").num();
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < n; i++) {
              String key = keys.get(Integer.toString(i)).toString();
              V6Value val = obj.get(key);
              if (val.tag() == V6Value.TAG_OBJ && val.ref() instanceof
                                                      V6Array) {
                V6Array arr = (V6Array)val.ref();
                int an = (int)arr.get("length").num();
                for (int j = 0; j < an; j++) {
                  if (sb.length() > 0)
                    sb.append(sep);
                  sb.append(escapeInternal(key))
                      .append(eq)
                      .append(escapeInternal(
                          arr.get(Integer.toString(j)).toString()));
                }
              } else {
                if (sb.length() > 0)
                  sb.append(sep);
                sb.append(escapeInternal(key))
                    .append(eq)
                    .append(escapeInternal(val.toString()));
              }
            }
            return str(sb.toString());
          }));
    o.set("encode", o.get("stringify"));

    return o;
  }
}

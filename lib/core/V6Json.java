public final class V6Json {
  private V6Json() {
  }

  private static final char[] HEX_DIGITS = "0123456789abcdef".toCharArray();

  public static V6Value parse(String text) {
    int[] pos = {0};
    skipWs(text, pos);
    V6Value result = parseValue(text, pos);
    skipWs(text, pos);
    if (pos[0] != text.length())
      throw new V6Throw(new V6Value(
          V6Value.TAG_STR, 0,
          "Unexpected non-whitespace character after JSON at position " +
              pos[0]));
    return result;
  }

  private static void skipWs(String s, int[] pos) {
    while (pos[0] < s.length()) {
      char c = s.charAt(pos[0]);
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        pos[0]++;
      else
        break;
    }
  }

  private static V6Value parseValue(String s, int[] pos) {
    skipWs(s, pos);
    if (pos[0] >= s.length())
      throw new V6Throw(
          new V6Value(V6Value.TAG_STR, 0, "Unexpected end of JSON input"));
    char c = s.charAt(pos[0]);
    if (c == '{')
      return parseObject(s, pos);
    if (c == '[')
      return parseArray(s, pos);
    if (c == '"')
      return new V6Value(V6Value.TAG_STR, 0, parseString(s, pos));
    if (c == 't') {
      expectLiteral(s, pos, "true");
      return V6Value.TRUE;
    }
    if (c == 'f') {
      expectLiteral(s, pos, "false");
      return V6Value.FALSE;
    }
    if (c == 'n') {
      expectLiteral(s, pos, "null");
      return V6Value.NUL;
    }
    if (c == '-' || (c >= '0' && c <= '9'))
      return parseNumber(s, pos);
    throw new V6Throw(new V6Value(V6Value.TAG_STR, 0,
                                  "Unexpected token '" + c +
                                      "' in JSON at position " + pos[0]));
  }

  private static void expectLiteral(String s, int[] pos, String lit) {
    if (pos[0] + lit.length() > s.length() ||
        !s.regionMatches(pos[0], lit, 0, lit.length()))
      throw new V6Throw(
          new V6Value(V6Value.TAG_STR, 0,
                      "Unexpected token in JSON at position " + pos[0]));
    pos[0] += lit.length();
  }

  private static V6Value parseObject(String s, int[] pos) {
    pos[0]++;
    V6Object obj = new V6Object();
    skipWs(s, pos);
    if (pos[0] < s.length() && s.charAt(pos[0]) == '}') {
      pos[0]++;
      return new V6Value(V6Value.TAG_OBJ, 0, obj);
    }
    while (true) {
      skipWs(s, pos);
      if (pos[0] >= s.length() || s.charAt(pos[0]) != '"')
        throw new V6Throw(new V6Value(
            V6Value.TAG_STR, 0,
            "Expected string key in JSON object at position " + pos[0]));
      String key = parseString(s, pos);
      skipWs(s, pos);
      if (pos[0] >= s.length() || s.charAt(pos[0]) != ':')
        throw new V6Throw(
            new V6Value(V6Value.TAG_STR, 0,
                        "Expected ':' in JSON object at position " + pos[0]));
      pos[0]++;
      V6Value val = parseValue(s, pos);
      obj.set(key, val);
      skipWs(s, pos);
      if (pos[0] >= s.length())
        throw new V6Throw(
            new V6Value(V6Value.TAG_STR, 0, "Unexpected end of JSON input"));
      char c = s.charAt(pos[0]);
      if (c == ',') {
        pos[0]++;
        continue;
      }
      if (c == '}') {
        pos[0]++;
        break;
      }
      throw new V6Throw(new V6Value(
          V6Value.TAG_STR, 0,
          "Expected ',' or '}' in JSON object at position " + pos[0]));
    }
    return new V6Value(V6Value.TAG_OBJ, 0, obj);
  }

  private static V6Value parseArray(String s, int[] pos) {
    pos[0]++;
    V6Array arr = new V6Array();
    skipWs(s, pos);
    if (pos[0] < s.length() && s.charAt(pos[0]) == ']') {
      pos[0]++;
      return new V6Value(V6Value.TAG_OBJ, 0, arr);
    }
    while (true) {
      V6Value val = parseValue(s, pos);
      arr.push(val);
      skipWs(s, pos);
      if (pos[0] >= s.length())
        throw new V6Throw(
            new V6Value(V6Value.TAG_STR, 0, "Unexpected end of JSON input"));
      char c = s.charAt(pos[0]);
      if (c == ',') {
        pos[0]++;
        continue;
      }
      if (c == ']') {
        pos[0]++;
        break;
      }
      throw new V6Throw(new V6Value(
          V6Value.TAG_STR, 0,
          "Expected ',' or ']' in JSON array at position " + pos[0]));
    }
    return new V6Value(V6Value.TAG_OBJ, 0, arr);
  }

  private static String parseString(String s, int[] pos) {
    pos[0]++;
    StringBuilder sb = new StringBuilder();
    while (true) {
      if (pos[0] >= s.length())
        throw new V6Throw(
            new V6Value(V6Value.TAG_STR, 0, "Unterminated string in JSON"));
      char c = s.charAt(pos[0]);
      if (c == '"') {
        pos[0]++;
        break;
      }
      if (c == '\\') {
        pos[0]++;
        if (pos[0] >= s.length())
          throw new V6Throw(
              new V6Value(V6Value.TAG_STR, 0, "Unterminated string in JSON"));
        char e = s.charAt(pos[0]);
        switch (e) {
        case '"':
          sb.append('"');
          break;
        case '\\':
          sb.append('\\');
          break;
        case '/':
          sb.append('/');
          break;
        case 'b':
          sb.append('\b');
          break;
        case 'f':
          sb.append('\f');
          break;
        case 'n':
          sb.append('\n');
          break;
        case 'r':
          sb.append('\r');
          break;
        case 't':
          sb.append('\t');
          break;
        case 'u':
          if (pos[0] + 4 >= s.length())
            throw new V6Throw(new V6Value(
                V6Value.TAG_STR, 0, "Invalid unicode escape in JSON string"));
          sb.append(
              (char)Integer.parseInt(s.substring(pos[0] + 1, pos[0] + 5), 16));
          pos[0] += 4;
          break;
        default:
          throw new V6Throw(new V6Value(
              V6Value.TAG_STR, 0, "Invalid escape character in JSON string"));
        }
        pos[0]++;
      } else if (c < 0x20) {
        throw new V6Throw(
            new V6Value(V6Value.TAG_STR, 0,
                        "Bad control character in string literal in JSON"));
      } else {
        sb.append(c);
        pos[0]++;
      }
    }
    return sb.toString();
  }

  private static V6Value parseNumber(String s, int[] pos) {
    int start = pos[0];
    if (s.charAt(pos[0]) == '-')
      pos[0]++;
    if (pos[0] >= s.length() || !Character.isDigit(s.charAt(pos[0])))
      throw new V6Throw(new V6Value(
          V6Value.TAG_STR, 0, "Invalid number in JSON at position " + pos[0]));
    if (s.charAt(pos[0]) == '0') {
      pos[0]++;
    } else {
      while (pos[0] < s.length() && Character.isDigit(s.charAt(pos[0])))
        pos[0]++;
    }
    if (pos[0] < s.length() && s.charAt(pos[0]) == '.') {
      pos[0]++;
      if (pos[0] >= s.length() || !Character.isDigit(s.charAt(pos[0])))
        throw new V6Throw(
            new V6Value(V6Value.TAG_STR, 0,
                        "Invalid number in JSON at position " + pos[0]));
      while (pos[0] < s.length() && Character.isDigit(s.charAt(pos[0])))
        pos[0]++;
    }
    if (pos[0] < s.length() &&
        (s.charAt(pos[0]) == 'e' || s.charAt(pos[0]) == 'E')) {
      pos[0]++;
      if (pos[0] < s.length() &&
          (s.charAt(pos[0]) == '+' || s.charAt(pos[0]) == '-'))
        pos[0]++;
      if (pos[0] >= s.length() || !Character.isDigit(s.charAt(pos[0])))
        throw new V6Throw(
            new V6Value(V6Value.TAG_STR, 0,
                        "Invalid number in JSON at position " + pos[0]));
      while (pos[0] < s.length() && Character.isDigit(s.charAt(pos[0])))
        pos[0]++;
    }
    return new V6Value(V6Value.TAG_NUM,
                       Double.parseDouble(s.substring(start, pos[0])), null);
  }

  public static V6Value stringify(V6Value value, V6Value replacerArg,
                                  V6Value spaceArg) {
    String indentUnit = "";
    if (spaceArg.tag() == V6Value.TAG_NUM) {
      int n = Math.max(0, Math.min(10, (int)spaceArg.toNumber()));
      StringBuilder sb = new StringBuilder();
      for (int i = 0; i < n; i++)
        sb.append(' ');
      indentUnit = sb.toString();
    } else if (spaceArg.tag() == V6Value.TAG_STR) {
      String s = spaceArg.toString();
      indentUnit = s.length() > 10 ? s.substring(0, 10) : s;
    }

    java.util.Set<String> allowedKeys = null;
    V6Callable replacerFn = null;
    if (replacerArg.tag() == V6Value.TAG_FUNC) {
      replacerFn = replacerArg.asCallable();
    } else if (replacerArg.tag() == V6Value.TAG_OBJ &&
               replacerArg.ref() instanceof V6Array) {
      allowedKeys = new java.util.LinkedHashSet<>();
      V6Array arr = (V6Array)replacerArg.ref();
      int n = (int)arr.get("length").num();
      for (int i = 0; i < n; i++)
        allowedKeys.add(arr.get(Integer.toString(i)).toString());
    }

    StringBuilder out = new StringBuilder();
    java.util.Set<Object> seen =
        java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<>());
    boolean ok =
        writeValue(out, value, replacerFn, allowedKeys, indentUnit, "", seen);
    if (!ok)
      return V6Value.UNDEF;
    return new V6Value(V6Value.TAG_STR, 0, out.toString());
  }

  private static V6Value applyToJSON(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Object) {
      V6Object o = (V6Object)v.ref();
      V6Value toJson = o.get("toJSON");
      if (toJson.tag() == V6Value.TAG_FUNC)
        return toJson.asCallable().call(v, new V6Value[0]);
    }
    return v;
  }

  private static boolean writeValue(StringBuilder out, V6Value vIn,
                                    V6Callable replacer,
                                    java.util.Set<String> allowedKeys,
                                    String indentUnit, String currentIndent,
                                    java.util.Set<Object> seen) {
    V6Value v = applyToJSON(vIn);
    switch (v.tag()) {
    case V6Value.TAG_UNDEF:
    case V6Value.TAG_FUNC:
      return false;
    case V6Value.TAG_BIGINT:
      throw new V6Throw(new V6Value(V6Value.TAG_STR, 0,
                                    "Do not know how to serialize a BigInt"));
    case V6Value.TAG_NULL:
      out.append("null");
      return true;
    case V6Value.TAG_BOOL:
      out.append(v.truthy() ? "true" : "false");
      return true;
    case V6Value.TAG_NUM: {
      double d = v.toNumber();
      if (Double.isNaN(d) || Double.isInfinite(d))
        out.append("null");
      else
        out.append(v.toString());
      return true;
    }
    case V6Value.TAG_STR:
      writeString(out, v.toString());
      return true;
    case V6Value.TAG_OBJ: {
      Object ref = v.ref();
      if (ref == null)
        return false;
      if (seen.contains(ref))
        throw new V6Throw(new V6Value(V6Value.TAG_STR, 0,
                                      "Converting circular structure to JSON"));
      seen.add(ref);
      String nextIndent = currentIndent + indentUnit;
      boolean pretty = !indentUnit.isEmpty();
      if (ref instanceof V6Array) {
        V6Array arr = (V6Array)ref;
        int n = (int)arr.get("length").num();
        if (n == 0) {
          out.append("[]");
        } else {
          out.append('[');
          for (int i = 0; i < n; i++) {
            if (i > 0)
              out.append(',');
            if (pretty)
              out.append('\n').append(nextIndent);
            V6Value elem = arr.get(Integer.toString(i));
            if (replacer != null)
              elem = replacer.call(
                  v, new V6Value[] {
                         new V6Value(V6Value.TAG_STR, 0, Integer.toString(i)),
                         elem});
            if (!writeValue(out, elem, replacer, allowedKeys, indentUnit,
                            nextIndent, seen))
              out.append("null");
          }
          if (pretty)
            out.append('\n').append(currentIndent);
          out.append(']');
        }
      } else {
        V6Object obj = (V6Object)ref;
        V6Array keys = obj.enumKeys();
        int n = (int)keys.get("length").num();
        StringBuilder body = new StringBuilder();
        int written = 0;
        for (int i = 0; i < n; i++) {
          String key = keys.get(Integer.toString(i)).toString();
          if (allowedKeys != null && !allowedKeys.contains(key))
            continue;
          V6Value propVal = obj.get(key);
          if (replacer != null)
            propVal = replacer.call(
                v,
                new V6Value[] {new V6Value(V6Value.TAG_STR, 0, key), propVal});
          StringBuilder valOut = new StringBuilder();
          if (!writeValue(valOut, propVal, replacer, allowedKeys, indentUnit,
                          nextIndent, seen))
            continue;
          if (written > 0)
            body.append(',');
          if (pretty)
            body.append('\n').append(nextIndent);
          writeString(body, key);
          body.append(':');
          if (pretty)
            body.append(' ');
          body.append(valOut);
          written++;
        }
        if (written == 0) {
          out.append("{}");
        } else {
          out.append('{').append(body);
          if (pretty)
            out.append('\n').append(currentIndent);
          out.append('}');
        }
      }
      seen.remove(ref);
      return true;
    }
    default:
      return false;
    }
  }

  private static void writeString(StringBuilder out, String s) {
    out.append('"');
    for (int i = 0; i < s.length(); i++) {
      char c = s.charAt(i);
      switch (c) {
      case '"':
        out.append("\\\"");
        break;
      case '\\':
        out.append("\\\\");
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      case '\b':
        out.append("\\b");
        break;
      case '\f':
        out.append("\\f");
        break;
      default:
        if (c < 0x20) {
          out.append("\\u00");
          out.append(HEX_DIGITS[(c >>> 4) & 0xf]);
          out.append(HEX_DIGITS[c & 0xf]);
        } else {
          out.append(c);
        }
      }
    }
    out.append('"');
  }
}

import java.util.regex.Pattern;

public final class V6Regex extends V6Object {
  public final Pattern pattern;
  public final boolean global;
  public final boolean sticky;

  public V6Regex(String source, String flags) {
    int f = 0;
    if (flags.indexOf('i') >= 0)
      f |= Pattern.CASE_INSENSITIVE | Pattern.UNICODE_CASE;
    if (flags.indexOf('m') >= 0)
      f |= Pattern.MULTILINE;
    if (flags.indexOf('s') >= 0)
      f |= Pattern.DOTALL;
    this.global = flags.indexOf('g') >= 0;
    this.sticky = flags.indexOf('y') >= 0;
    this.pattern = Pattern.compile(translate(source), f);
    setProto(V6Builtins.REGEXP_PROTOTYPE);
    set("source", new V6Value(V6Value.TAG_STR, 0, source));
    set("flags", new V6Value(V6Value.TAG_STR, 0, flags));
    set("global", global ? V6Value.TRUE : V6Value.FALSE);
    set("ignoreCase", flags.indexOf('i') >= 0 ? V6Value.TRUE : V6Value.FALSE);
    set("multiline", flags.indexOf('m') >= 0 ? V6Value.TRUE : V6Value.FALSE);
    set("sticky", sticky ? V6Value.TRUE : V6Value.FALSE);
    set("unicode", flags.indexOf('u') >= 0 ? V6Value.TRUE : V6Value.FALSE);
    set("lastIndex", new V6Value(V6Value.TAG_NUM, 0, null));
  }

  private static String translate(String s) {
    StringBuilder out = new StringBuilder();
    int n = s.length();
    boolean inClass = false;
    for (int i = 0; i < n; i++) {
      char c = s.charAt(i);
      if (c == '\\' && i + 1 < n &&
          (s.charAt(i + 1) == 'p' || s.charAt(i + 1) == 'P') && i + 2 < n &&
          s.charAt(i + 2) == '{') {
        int close = s.indexOf('}', i + 3);
        if (close > 0) {
          String prop = s.substring(i + 3, close);
          String javaEquiv = null;
          if (prop.equals("ID_Start"))
            javaEquiv = "\\p{L}";
          else if (prop.equals("ID_Continue"))
            javaEquiv = "\\p{L}\\p{Mn}\\p{Mc}\\p{Nd}\\p{Pc}";
          if (javaEquiv != null) {
            out.append(inClass ? javaEquiv : "[" + javaEquiv + "]");
            i = close;
            continue;
          }
        }
      }
      if (c == '\\' && i + 1 < n) {
        out.append(c).append(s.charAt(i + 1));
        i++;
        continue;
      }
      if (!inClass && c == '[' && i + 2 < n && s.charAt(i + 1) == '^' &&
          s.charAt(i + 2) == ']') {
        out.append("[\\s\\S]");
        i += 2;
        continue;
      }
      if (!inClass && c == '[') {
        inClass = true;
        out.append(c);
        continue;
      }
      if (inClass && c == '[') {
        out.append("\\[");
        continue;
      }
      if (inClass && c == ']') {
        inClass = false;
        out.append(c);
        continue;
      }
      out.append(c);
    }
    return out.toString();
  }
}

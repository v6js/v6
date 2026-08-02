import java.util.ArrayList;
import java.util.List;

public final class V6Path {
  private final boolean isWindows;
  private final char sep;
  private final String sepStr;
  private final String delimiter;

  private V6Path(boolean isWindows) {
    this.isWindows = isWindows;
    this.sep = isWindows ? '\\' : '/';
    this.sepStr = String.valueOf(sep);
    this.delimiter = isWindows ? ";" : ":";
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static String s(V6Value v) {
    return v.toString();
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private boolean isSlash(char c) {
    return c == '/' || (isWindows && c == '\\');
  }

  private String driveLetter(String p) {
    if (isWindows && p.length() >= 2 && Character.isLetter(p.charAt(0)) &&
        p.charAt(1) == ':')
      return p.substring(0, 2);
    return "";
  }

  private boolean isAbsoluteInternal(String p) {
    String drive = driveLetter(p);
    String rest = p.substring(drive.length());
    return !rest.isEmpty() && isSlash(rest.charAt(0));
  }

  private String normalizeInternal(String p) {
    if (p.isEmpty())
      return ".";
    String drive = driveLetter(p);
    String rest = p.substring(drive.length());
    boolean abs = !rest.isEmpty() && isSlash(rest.charAt(0));
    boolean trailingSlash =
        rest.length() > 1 && isSlash(rest.charAt(rest.length() - 1));

    List<String> stack = new ArrayList<>();
    StringBuilder cur = new StringBuilder();
    for (int i = 0; i <= rest.length(); i++) {
      char c = i < rest.length() ? rest.charAt(i) : 0;
      if (i == rest.length() || isSlash(c)) {
        String part = cur.toString();
        cur.setLength(0);
        if (part.isEmpty() || part.equals("."))
          continue;
        if (part.equals("..")) {
          if (!stack.isEmpty() && !stack.get(stack.size() - 1).equals(".."))
            stack.remove(stack.size() - 1);
          else if (!abs)
            stack.add("..");
        } else {
          stack.add(part);
        }
      } else {
        cur.append(c);
      }
    }

    String joined = String.join(sepStr, stack);
    String result = drive + (abs ? sepStr : "") + joined;
    if (result.isEmpty())
      result = ".";
    if (trailingSlash && !result.endsWith(sepStr))
      result = result + sepStr;
    return result;
  }

  private String joinInternal(String[] parts) {
    StringBuilder sb = new StringBuilder();
    for (String part : parts) {
      if (part.isEmpty())
        continue;
      if (sb.length() > 0 && !isSlash(sb.charAt(sb.length() - 1)))
        sb.append(sep);
      sb.append(part);
    }
    return sb.length() == 0 ? "." : normalizeInternal(sb.toString());
  }

  private static String cwd() {
    return System.getProperty("user.dir", ".");
  }

  private String resolveInternal(String[] parts) {
    String resolved = "";
    boolean resolvedAbs = false;
    for (int i = parts.length - 1; i >= -1 && !resolvedAbs; i--) {
      String part = i >= 0 ? parts[i] : cwd();
      if (part.isEmpty())
        continue;
      resolved = part + sepStr + resolved;
      resolvedAbs = isAbsoluteInternal(part);
    }
    String normalized = normalizeInternal(resolved);
    if (normalized.endsWith(sepStr) &&
        normalized.length() > driveLetter(normalized).length() + 1)
      normalized = normalized.substring(0, normalized.length() - 1);
    return normalized;
  }

  private String dirnameInternal(String p) {
    String drive = driveLetter(p);
    String rest = p.substring(drive.length());
    int end = rest.length();
    while (end > 0 && isSlash(rest.charAt(end - 1)))
      end--;
    int slash = -1;
    for (int i = end - 1; i >= 0; i--) {
      if (isSlash(rest.charAt(i))) {
        slash = i;
        break;
      }
    }
    if (slash < 0)
      return drive.isEmpty() ? "." : drive + sepStr;
    if (slash == 0)
      return drive + sepStr;
    return drive + rest.substring(0, slash);
  }

  private String basenameInternal(String p, String ext) {
    String rest = p.substring(driveLetter(p).length());
    int end = rest.length();
    while (end > 0 && isSlash(rest.charAt(end - 1)))
      end--;
    int slash = -1;
    for (int i = end - 1; i >= 0; i--) {
      if (isSlash(rest.charAt(i))) {
        slash = i;
        break;
      }
    }
    String base = rest.substring(slash + 1, end);
    if (ext != null && !ext.isEmpty() && base.endsWith(ext) &&
        !base.equals(ext))
      base = base.substring(0, base.length() - ext.length());
    return base;
  }

  private String extnameInternal(String p) {
    String base = basenameInternal(p, null);
    int dot = base.lastIndexOf('.');
    if (dot <= 0)
      return "";
    return base.substring(dot);
  }

  private String relativeInternal(String from, String to) {
    String fromAbs = resolveInternal(new String[] {from});
    String toAbs = resolveInternal(new String[] {to});
    if (fromAbs.equals(toAbs))
      return "";
    String[] fromParts = splitParts(fromAbs);
    String[] toParts = splitParts(toAbs);
    int common = 0;
    while (common < fromParts.length && common < toParts.length &&
           fromParts[common].equals(toParts[common]))
      common++;
    StringBuilder sb = new StringBuilder();
    for (int i = common; i < fromParts.length; i++) {
      if (sb.length() > 0)
        sb.append(sep);
      sb.append("..");
    }
    for (int i = common; i < toParts.length; i++) {
      if (sb.length() > 0)
        sb.append(sep);
      sb.append(toParts[i]);
    }
    return sb.toString();
  }

  private String[] splitParts(String absPath) {
    String rest = absPath.substring(driveLetter(absPath).length());
    List<String> parts = new ArrayList<>();
    StringBuilder cur = new StringBuilder();
    for (int i = 0; i <= rest.length(); i++) {
      char c = i < rest.length() ? rest.charAt(i) : 0;
      if (i == rest.length() || isSlash(c)) {
        if (cur.length() > 0)
          parts.add(cur.toString());
        cur.setLength(0);
      } else {
        cur.append(c);
      }
    }
    return parts.toArray(new String[0]);
  }

  private V6Object buildObject() {
    V6Object o = new V6Object();
    o.set("sep", str(sepStr));
    o.set("delimiter", str(delimiter));

    o.set("join", fn((thisArg, args) -> {
            String[] parts = new String[args.length];
            for (int i = 0; i < args.length; i++)
              parts[i] = s(args[i]);
            return str(joinInternal(parts));
          }));

    o.set("resolve", fn((thisArg, args) -> {
            String[] parts = new String[args.length];
            for (int i = 0; i < args.length; i++)
              parts[i] = s(args[i]);
            return str(resolveInternal(parts));
          }));

    o.set("normalize",
          fn((thisArg,
              args) -> str(normalizeInternal(s(V6Value.argAt(args, 0))))));

    o.set("isAbsolute", fn((thisArg, args) -> {
            boolean b = isAbsoluteInternal(s(V6Value.argAt(args, 0)));
            return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
          }));

    o.set(
        "dirname",
        fn((thisArg, args) -> str(dirnameInternal(s(V6Value.argAt(args, 0))))));

    o.set("basename", fn((thisArg, args) -> {
            String p = s(V6Value.argAt(args, 0));
            String ext = args.length > 1 ? s(args[1]) : null;
            return str(basenameInternal(p, ext));
          }));

    o.set(
        "extname",
        fn((thisArg, args) -> str(extnameInternal(s(V6Value.argAt(args, 0))))));

    o.set("relative",
          fn((thisArg, args)
                 -> str(relativeInternal(s(V6Value.argAt(args, 0)),
                                         s(V6Value.argAt(args, 1))))));

    o.set("parse", fn((thisArg, args) -> {
            String p = s(V6Value.argAt(args, 0));
            String drive = driveLetter(p);
            String rest = p.substring(drive.length());
            String root = (!rest.isEmpty() && isSlash(rest.charAt(0)))
                              ? drive + sepStr
                              : drive;
            String dir = dirnameInternal(p);
            String base = basenameInternal(p, null);
            String ext = extnameInternal(p);
            String name = ext.isEmpty()
                              ? base
                              : base.substring(0, base.length() - ext.length());
            V6Object result = new V6Object();
            result.set("root", str(root));
            result.set("dir", str(dir));
            result.set("base", str(base));
            result.set("ext", str(ext));
            result.set("name", str(name));
            return new V6Value(V6Value.TAG_OBJ, 0, result);
          }));

    o.set(
        "format", fn((thisArg, args) -> {
          V6Value arg = V6Value.argAt(args, 0);
          if (arg.tag() != V6Value.TAG_OBJ)
            return str("");
          V6Object obj = (V6Object)arg.ref();
          String dir = obj.get("dir").isUndefined() ? "" : s(obj.get("dir"));
          String root = obj.get("root").isUndefined() ? "" : s(obj.get("root"));
          String base = obj.get("base").isUndefined() ? "" : s(obj.get("base"));
          String name = obj.get("name").isUndefined() ? "" : s(obj.get("name"));
          String ext = obj.get("ext").isUndefined() ? "" : s(obj.get("ext"));
          if (base.isEmpty())
            base = name + ext;
          String d = !dir.isEmpty() ? dir : root;
          if (d.isEmpty())
            return str(base);
          return str(d.endsWith(sepStr) ? d + base : d + sepStr + base);
        }));

    return o;
  }

  public static V6Object build() {
    boolean hostIsWindows =
        System.getProperty("os.name", "").toLowerCase().contains("win");
    V6Object def = new V6Path(hostIsWindows).buildObject();
    V6Object win32 = new V6Path(true).buildObject();
    V6Object posix = new V6Path(false).buildObject();
    win32.set("win32", new V6Value(V6Value.TAG_OBJ, 0, win32));
    win32.set("posix", new V6Value(V6Value.TAG_OBJ, 0, posix));
    posix.set("win32", new V6Value(V6Value.TAG_OBJ, 0, win32));
    posix.set("posix", new V6Value(V6Value.TAG_OBJ, 0, posix));
    def.set("win32", new V6Value(V6Value.TAG_OBJ, 0, win32));
    def.set("posix", new V6Value(V6Value.TAG_OBJ, 0, posix));
    return def;
  }
}

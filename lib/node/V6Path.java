import java.util.ArrayList;
import java.util.List;

public final class V6Path {
  private V6Path() {}

  private static final boolean IS_WINDOWS =
      System.getProperty("os.name", "").toLowerCase().contains("win");
  private static final char SEP = IS_WINDOWS ? '\\' : '/';
  private static final String SEP_STR = String.valueOf(SEP);
  private static final String DELIMITER = IS_WINDOWS ? ";" : ":";

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static String s(V6Value v) {
    return v.toString();
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static boolean isSlash(char c) {
    return c == '/' || (IS_WINDOWS && c == '\\');
  }

  private static String driveLetter(String p) {
    if (IS_WINDOWS && p.length() >= 2 && Character.isLetter(p.charAt(0)) &&
        p.charAt(1) == ':')
      return p.substring(0, 2);
    return "";
  }

  private static boolean isAbsoluteInternal(String p) {
    String drive = driveLetter(p);
    String rest = p.substring(drive.length());
    return !rest.isEmpty() && isSlash(rest.charAt(0));
  }

  private static String normalizeInternal(String p) {
    if (p.isEmpty())
      return ".";
    String drive = driveLetter(p);
    String rest = p.substring(drive.length());
    boolean abs = !rest.isEmpty() && isSlash(rest.charAt(0));
    boolean trailingSlash = rest.length() > 1 && isSlash(rest.charAt(rest.length() - 1));

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

    String joined = String.join(SEP_STR, stack);
    String result = drive + (abs ? SEP_STR : "") + joined;
    if (result.isEmpty())
      result = ".";
    if (trailingSlash && !result.endsWith(SEP_STR))
      result = result + SEP_STR;
    return result;
  }

  private static String joinInternal(String[] parts) {
    StringBuilder sb = new StringBuilder();
    for (String part : parts) {
      if (part.isEmpty())
        continue;
      if (sb.length() > 0 && !isSlash(sb.charAt(sb.length() - 1)))
        sb.append(SEP);
      sb.append(part);
    }
    return sb.length() == 0 ? "." : normalizeInternal(sb.toString());
  }

  private static String cwd() {
    return System.getProperty("user.dir", ".");
  }

  private static String resolveInternal(String[] parts) {
    String resolved = "";
    boolean resolvedAbs = false;
    for (int i = parts.length - 1; i >= -1 && !resolvedAbs; i--) {
      String part = i >= 0 ? parts[i] : cwd();
      if (part.isEmpty())
        continue;
      resolved = part + SEP_STR + resolved;
      resolvedAbs = isAbsoluteInternal(part);
    }
    String normalized = normalizeInternal(resolved);
    if (normalized.endsWith(SEP_STR) && normalized.length() > driveLetter(normalized).length() + 1)
      normalized = normalized.substring(0, normalized.length() - 1);
    return normalized;
  }

  private static String dirnameInternal(String p) {
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
      return drive.isEmpty() ? "." : drive + SEP_STR;
    if (slash == 0)
      return drive + SEP_STR;
    return drive + rest.substring(0, slash);
  }

  private static String basenameInternal(String p, String ext) {
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
    if (ext != null && !ext.isEmpty() && base.endsWith(ext) && !base.equals(ext))
      base = base.substring(0, base.length() - ext.length());
    return base;
  }

  private static String extnameInternal(String p) {
    String base = basenameInternal(p, null);
    int dot = base.lastIndexOf('.');
    if (dot <= 0)
      return "";
    return base.substring(dot);
  }

  private static String relativeInternal(String from, String to) {
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
        sb.append(SEP);
      sb.append("..");
    }
    for (int i = common; i < toParts.length; i++) {
      if (sb.length() > 0)
        sb.append(SEP);
      sb.append(toParts[i]);
    }
    return sb.toString();
  }

  private static String[] splitParts(String absPath) {
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

  public static V6Object build() {
    V6Object o = new V6Object();
    o.set("sep", str(SEP_STR));
    o.set("delimiter", str(DELIMITER));

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

    o.set("normalize", fn((thisArg, args)
                              -> str(normalizeInternal(s(V6Value.argAt(args, 0))))));

    o.set("isAbsolute", fn((thisArg, args) -> {
            boolean b = isAbsoluteInternal(s(V6Value.argAt(args, 0)));
            return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
          }));

    o.set("dirname",
          fn((thisArg, args) -> str(dirnameInternal(s(V6Value.argAt(args, 0))))));

    o.set("basename", fn((thisArg, args) -> {
            String p = s(V6Value.argAt(args, 0));
            String ext = args.length > 1 ? s(args[1]) : null;
            return str(basenameInternal(p, ext));
          }));

    o.set("extname",
          fn((thisArg, args) -> str(extnameInternal(s(V6Value.argAt(args, 0))))));

    o.set("relative", fn((thisArg, args)
                             -> str(relativeInternal(s(V6Value.argAt(args, 0)),
                                                     s(V6Value.argAt(args, 1))))));

    o.set("parse", fn((thisArg, args) -> {
            String p = s(V6Value.argAt(args, 0));
            String drive = driveLetter(p);
            String rest = p.substring(drive.length());
            String root = (!rest.isEmpty() && isSlash(rest.charAt(0)))
                ? drive + SEP_STR
                : drive;
            String dir = dirnameInternal(p);
            String base = basenameInternal(p, null);
            String ext = extnameInternal(p);
            String name = ext.isEmpty() ? base : base.substring(0, base.length() - ext.length());
            V6Object result = new V6Object();
            result.set("root", str(root));
            result.set("dir", str(dir));
            result.set("base", str(base));
            result.set("ext", str(ext));
            result.set("name", str(name));
            return new V6Value(V6Value.TAG_OBJ, 0, result);
          }));

    o.set("format", fn((thisArg, args) -> {
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
            return str(d.endsWith(SEP_STR) ? d + base : d + SEP_STR + base);
          }));

    return o;
  }
}

public final class V6String extends V6Object {
  public static final V6String PROTOTYPE = new V6String();

  private V6String() {
    build();
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static String s(V6Value v) {
    return v.toString();
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static int normIndex(int idx, int len) {
    if (idx < 0)
      idx = Math.max(0, len + idx);
    return Math.min(idx, len);
  }

  private void build() {
    set("charAt", fn((thisArg, args) -> {
          String self = s(thisArg);
          int i = (int)V6Value.argAt(args, 0).toNumber();
          if (i < 0 || i >= self.length())
            return str("");
          return str(String.valueOf(self.charAt(i)));
        }));
    set("charCodeAt", fn((thisArg, args) -> {
          String self = s(thisArg);
          int i = (int)V6Value.argAt(args, 0).toNumber();
          if (i < 0 || i >= self.length())
            return num(Double.NaN);
          return num(self.charAt(i));
        }));
    set("codePointAt", fn((thisArg, args) -> {
          String self = s(thisArg);
          int i = (int)V6Value.argAt(args, 0).toNumber();
          if (i < 0 || i >= self.length())
            return UNDEF;
          return num(self.codePointAt(i));
        }));
    set("at", fn((thisArg, args) -> {
          String self = s(thisArg);
          int i = (int)V6Value.argAt(args, 0).toNumber();
          if (i < 0)
            i += self.length();
          if (i < 0 || i >= self.length())
            return UNDEF;
          return str(String.valueOf(self.charAt(i)));
        }));
    set("indexOf", fn((thisArg, args) -> {
          String self = s(thisArg);
          String needle = s(V6Value.argAt(args, 0));
          int from = args.length > 1 ? (int)args[1].toNumber() : 0;
          return num(self.indexOf(needle, from));
        }));
    set("lastIndexOf", fn((thisArg, args)
                              -> num(s(thisArg).lastIndexOf(
                                  s(V6Value.argAt(args, 0))))));
    set("includes", fn((thisArg, args)
                           -> bool(s(thisArg).contains(
                               s(V6Value.argAt(args, 0))))));
    set("startsWith", fn((thisArg, args) -> {
          String self = s(thisArg);
          String needle = s(V6Value.argAt(args, 0));
          int from = args.length > 1 ? (int)args[1].toNumber() : 0;
          return bool(self.startsWith(needle, from));
        }));
    set("endsWith", fn((thisArg, args) -> {
          String self = s(thisArg);
          String needle = s(V6Value.argAt(args, 0));
          int end = args.length > 1 ? (int)args[1].toNumber() : self.length();
          end = Math.max(0, Math.min(end, self.length()));
          return bool(self.substring(0, end).endsWith(needle));
        }));
    set("slice", fn((thisArg, args) -> {
          String self = s(thisArg);
          int len = self.length();
          int start = args.length > 0 ? normIndex((int)args[0].toNumber(), len) : 0;
          int end = args.length > 1 && !args[1].isUndefined()
                        ? normIndex((int)args[1].toNumber(), len)
                        : len;
          if (start >= end)
            return str("");
          return str(self.substring(start, end));
        }));
    set("substring", fn((thisArg, args) -> {
          String self = s(thisArg);
          int len = self.length();
          int start = args.length > 0
                          ? Math.max(0, Math.min(len, (int)args[0].toNumber()))
                          : 0;
          int end = args.length > 1 && !args[1].isUndefined()
                        ? Math.max(0, Math.min(len, (int)args[1].toNumber()))
                        : len;
          if (start > end) {
            int tmp = start;
            start = end;
            end = tmp;
          }
          return str(self.substring(start, end));
        }));
    set("toUpperCase", fn((thisArg, args) -> str(s(thisArg).toUpperCase())));
    set("toLowerCase", fn((thisArg, args) -> str(s(thisArg).toLowerCase())));
    set("trim", fn((thisArg, args) -> str(s(thisArg).strip())));
    set("trimStart", fn((thisArg, args) -> str(s(thisArg).stripLeading())));
    set("trimEnd", fn((thisArg, args) -> str(s(thisArg).stripTrailing())));
    set("split", fn((thisArg, args) -> {
          String self = s(thisArg);
          V6Array result = new V6Array();
          if (args.length == 0 || args[0].isUndefined()) {
            result.push(str(self));
            return new V6Value(V6Value.TAG_OBJ, 0, result);
          }
          String sep = s(args[0]);
          if (sep.isEmpty()) {
            for (int i = 0; i < self.length(); i++)
              result.push(str(String.valueOf(self.charAt(i))));
          } else {
            int idx = 0;
            while (true) {
              int next = self.indexOf(sep, idx);
              if (next < 0) {
                result.push(str(self.substring(idx)));
                break;
              }
              result.push(str(self.substring(idx, next)));
              idx = next + sep.length();
            }
          }
          return new V6Value(V6Value.TAG_OBJ, 0, result);
        }));
    set("replace", fn((thisArg, args) -> {
          String self = s(thisArg);
          String search = s(V6Value.argAt(args, 0));
          String repl = s(V6Value.argAt(args, 1));
          int idx = self.indexOf(search);
          if (idx < 0)
            return str(self);
          return str(self.substring(0, idx) + repl +
                     self.substring(idx + search.length()));
        }));
    set("replaceAll", fn((thisArg, args) -> {
          String self = s(thisArg);
          String search = s(V6Value.argAt(args, 0));
          String repl = s(V6Value.argAt(args, 1));
          if (search.isEmpty())
            return str(self);
          return str(self.replace(search, repl));
        }));
    set("repeat", fn((thisArg, args) -> {
          int n = (int)V6Value.argAt(args, 0).toNumber();
          if (n < 0)
            throw new RuntimeException("Invalid count value");
          return str(s(thisArg).repeat(n));
        }));
    set("padStart", fn((thisArg, args) -> {
          String self = s(thisArg);
          int target = (int)V6Value.argAt(args, 0).toNumber();
          String pad = args.length > 1 ? s(args[1]) : " ";
          if (pad.isEmpty() || self.length() >= target)
            return str(self);
          StringBuilder sb = new StringBuilder();
          while (sb.length() < target - self.length())
            sb.append(pad);
          return str(sb.substring(0, target - self.length()) + self);
        }));
    set("padEnd", fn((thisArg, args) -> {
          String self = s(thisArg);
          int target = (int)V6Value.argAt(args, 0).toNumber();
          String pad = args.length > 1 ? s(args[1]) : " ";
          if (pad.isEmpty() || self.length() >= target)
            return str(self);
          StringBuilder sb = new StringBuilder(self);
          while (sb.length() < target)
            sb.append(pad);
          return str(sb.substring(0, target));
        }));
    set("concat", fn((thisArg, args) -> {
          StringBuilder sb = new StringBuilder(s(thisArg));
          for (V6Value v : args)
            sb.append(s(v));
          return str(sb.toString());
        }));
    set("toString", fn((thisArg, args) -> str(s(thisArg))));
    set("valueOf", fn((thisArg, args) -> str(s(thisArg))));
  }
}

import java.nio.charset.StandardCharsets;
import java.util.Base64;

public final class V6BufferConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6BufferConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    set("from",
        fn((thisArg, args) -> objValue(new V6Buffer(decodeFrom(args)))));
    set("alloc", fn((thisArg, args) -> {
          int size = (int)V6Value.argAt(args, 0).toNumber();
          byte[] bytes = new byte[Math.max(0, size)];
          if (args.length > 1) {
            V6Value fill = args[1];
            if (fill.tag() == V6Value.TAG_STR) {
              byte[] pattern = fill.toString().getBytes(StandardCharsets.UTF_8);
              if (pattern.length > 0)
                for (int i = 0; i < bytes.length; i++)
                  bytes[i] = pattern[i % pattern.length];
            } else {
              byte v = (byte)(int)fill.toNumber();
              java.util.Arrays.fill(bytes, v);
            }
          }
          return objValue(new V6Buffer(bytes));
        }));
    set("allocUnsafe", fn((thisArg, args) -> {
          int size = (int)V6Value.argAt(args, 0).toNumber();
          return objValue(new V6Buffer(new byte[Math.max(0, size)]));
        }));
    set("isBuffer", fn((thisArg, args) -> {
          V6Value v = V6Value.argAt(args, 0);
          boolean is =
              v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer;
          return new V6Value(V6Value.TAG_BOOL, is ? 1 : 0, null);
        }));
    set("byteLength",
        fn((thisArg, args)
               -> new V6Value(V6Value.TAG_NUM, decodeFrom(args).length, null)));
    set("concat", fn((thisArg, args) -> {
          V6Value listVal = V6Value.argAt(args, 0);
          V6Object list = (V6Object)listVal.ref();
          int total = (int)list.get("length").num();
          java.io.ByteArrayOutputStream out =
              new java.io.ByteArrayOutputStream();
          for (int i = 0; i < total; i++) {
            V6Value item = list.get(Integer.toString(i));
            if (item.tag() == V6Value.TAG_OBJ && item.ref() instanceof V6Buffer)
              out.writeBytes(((V6Buffer)item.ref()).toBytes());
          }
          return objValue(new V6Buffer(out.toByteArray()));
        }));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    if (first.tag() == V6Value.TAG_NUM)
      return objValue(
          new V6Buffer(new byte[Math.max(0, (int)first.toNumber())]));
    return objValue(new V6Buffer(decodeFrom(args)));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static byte[] decodeFrom(V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    String encoding = args.length > 1 ? args[1].toString() : "utf8";
    if (first.tag() == V6Value.TAG_STR)
      return decodeString(first.toString(), encoding);
    if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof V6Buffer)
      return ((V6Buffer)first.ref()).toBytes();
    if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof
                                              V6ArrayBufferObject)
      return ((V6ArrayBufferObject)first.ref()).data;
    if (first.tag() == V6Value.TAG_OBJ) {
      V6Object arr = (V6Object)first.ref();
      int n = (int)arr.get("length").num();
      byte[] out = new byte[n];
      for (int i = 0; i < n; i++)
        out[i] = (byte)(int)arr.get(Integer.toString(i)).toNumber();
      return out;
    }
    return new byte[0];
  }

  static byte[] decodeString(String s, String encoding) {
    switch (encoding.toLowerCase()) {
    case "hex":
      int n = s.length() / 2;
      byte[] out = new byte[n];
      for (int i = 0; i < n; i++)
        out[i] = (byte)Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
      return out;
    case "base64":
      return Base64.getDecoder().decode(s);
    case "ascii":
    case "latin1":
    case "binary":
      return s.getBytes(StandardCharsets.ISO_8859_1);
    default:
      return s.getBytes(StandardCharsets.UTF_8);
    }
  }

  static String encodeBytes(byte[] bytes, String encoding) {
    switch (encoding.toLowerCase()) {
    case "hex":
      StringBuilder sb = new StringBuilder();
      for (byte b : bytes)
        sb.append(String.format("%02x", b));
      return sb.toString();
    case "base64":
      return Base64.getEncoder().encodeToString(bytes);
    case "ascii":
    case "latin1":
    case "binary":
      return new String(bytes, StandardCharsets.ISO_8859_1);
    default:
      return new String(bytes, StandardCharsets.UTF_8);
    }
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.set("toString", fn((thisArg, args) -> {
            V6Buffer buf = (V6Buffer)thisArg.ref();
            String encoding = args.length > 0 ? args[0].toString() : "utf8";
            return str(encodeBytes(buf.toBytes(), encoding));
          }));
    o.set("write", fn((thisArg, args) -> {
            V6Buffer buf = (V6Buffer)thisArg.ref();
            String s = V6Value.argAt(args, 0).toString();
            String encoding =
                args.length > 1 && args[1].tag() == V6Value.TAG_STR
                    ? args[1].toString()
                    : "utf8";
            byte[] bytes = decodeString(s, encoding);
            int n = (int)buf.get("length").num();
            int written = Math.min(bytes.length, n);
            for (int i = 0; i < written; i++)
              buf.set(Integer.toString(i),
                      new V6Value(V6Value.TAG_NUM, bytes[i] & 0xFF, null));
            return new V6Value(V6Value.TAG_NUM, written, null);
          }));
    o.set("slice", fn((thisArg, args) -> {
            V6Buffer buf = (V6Buffer)thisArg.ref();
            byte[] bytes = buf.toBytes();
            int start = args.length > 0
                            ? normIndex((int)args[0].toNumber(), bytes.length)
                            : 0;
            int end = args.length > 1
                          ? normIndex((int)args[1].toNumber(), bytes.length)
                          : bytes.length;
            if (start > end)
              start = end;
            byte[] out = java.util.Arrays.copyOfRange(bytes, start, end);
            return objValue(new V6Buffer(out));
          }));
    o.set("equals", fn((thisArg, args) -> {
            V6Buffer buf = (V6Buffer)thisArg.ref();
            V6Value other = V6Value.argAt(args, 0);
            boolean eq = other.tag() == V6Value.TAG_OBJ &&
                         other.ref() instanceof V6Buffer &&
                         java.util.Arrays.equals(
                             buf.toBytes(), ((V6Buffer)other.ref()).toBytes());
            return new V6Value(V6Value.TAG_BOOL, eq ? 1 : 0, null);
          }));
    o.set("toJSON", fn((thisArg, args) -> {
            V6Buffer buf = (V6Buffer)thisArg.ref();
            V6Object result = new V6Object();
            result.set("type", str("Buffer"));
            V6Array data = new V6Array();
            for (byte b : buf.toBytes())
              data.push(new V6Value(V6Value.TAG_NUM, b & 0xFF, null));
            result.set("data", objValue(data));
            return objValue(result);
          }));
    return o;
  }

  private static int normIndex(int idx, int len) {
    if (idx < 0)
      idx = Math.max(0, len + idx);
    return Math.min(idx, len);
  }
}

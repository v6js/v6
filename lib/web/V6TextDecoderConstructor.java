public final class V6TextDecoderConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6TextDecoderConstructor() {
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
  private static final char BOM_CHAR = '\uFEFF';

  static String normalizeLabel(String label) {
    String l = label.toLowerCase().trim();
    switch (l) {
    case "utf-8":
    case "utf8":
    case "unicode-1-1-utf-8":
      return "utf8";
    case "utf-16le":
    case "utf-16":
      return "utf16le";
    case "iso-8859-1":
    case "latin1":
    case "ascii":
    case "windows-1252":
      return "latin1";
    default:
      return "utf8";
    }
  }

  @Override
  public V6Object allocate() {
    return new V6TextDecoderObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6TextDecoderObject d = new V6TextDecoderObject();
    d.setProto(PROTOTYPE);
    initInstance(d, args);
    return objValue(d);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6TextDecoderObject d = (V6TextDecoderObject)instance;
    String label = args.length > 0 && !args[0].isUndefined()
                       ? args[0].toString()
                       : "utf-8";
    d.encoding = normalizeLabel(label);
    d.fatal = false;
    d.ignoreBOM = false;
    if (args.length > 1 && args[1].tag() == V6Value.TAG_OBJ &&
        args[1].ref() instanceof V6Object) {
      V6Object opts = (V6Object)args[1].ref();
      d.fatal = opts.get("fatal").truthy();
      d.ignoreBOM = opts.get("ignoreBOM").truthy();
    }
    d.decoder =
        new V6StringDecoderObject(d.encoding.equals("utf16le")  ? "utf16le"
                                  : d.encoding.equals("latin1") ? "latin1"
                                                                : "utf8");
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return new byte[0];
  }

  private static V6TextDecoderObject self(V6Value t) {
    return (V6TextDecoderObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.defineGetter("encoding", (t, a) -> str(self(t).encoding));
    o.defineGetter(
        "fatal",
        (t, a) -> new V6Value(V6Value.TAG_BOOL, self(t).fatal ? 1 : 0, null));
    o.defineGetter(
        "ignoreBOM",
        (t,
         a) -> new V6Value(V6Value.TAG_BOOL, self(t).ignoreBOM ? 1 : 0, null));

    o.set("decode", fn((t, a) -> {
            V6TextDecoderObject d = self(t);
            byte[] bytes = a.length > 0 && !a[0].isUndefined() ? bytesOf(a[0])
                                                               : new byte[0];
            boolean streaming = a.length > 1 && a[1].tag() == V6Value.TAG_OBJ &&
                                a[1].ref() instanceof V6Object &&
                                ((V6Object)a[1].ref()).get("stream").truthy();
            String result =
                streaming ? d.decoder.write(bytes) : d.decoder.end(bytes);
            if (!d.sawFirstChunk) {
              d.sawFirstChunk = true;
              if (!d.ignoreBOM && result.length() > 0 &&
                  result.charAt(0) == BOM_CHAR)
                result = result.substring(1);
            }
            if (!streaming)
              d.sawFirstChunk = false;
            return str(result);
          }));

    return o;
  }
}

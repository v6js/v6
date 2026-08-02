import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;

public class V6BlobConstructor extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6BlobConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  static byte[] partBytes(V6Value part) {
    if (part.tag() == V6Value.TAG_STR)
      return part.toString().getBytes(StandardCharsets.UTF_8);
    if (part.tag() == V6Value.TAG_OBJ && part.ref() instanceof V6Buffer)
      return ((V6Buffer)part.ref()).toBytes();
    if (part.tag() == V6Value.TAG_OBJ && part.ref() instanceof V6BlobObject)
      return ((V6BlobObject)part.ref()).data;
    if (part.tag() == V6Value.TAG_OBJ && part.ref() instanceof
                                             V6ArrayBufferObject)
      return ((V6ArrayBufferObject)part.ref()).data;
    return part.toString().getBytes(StandardCharsets.UTF_8);
  }

  static byte[] concatParts(V6Value partsVal) {
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    if (partsVal.tag() == V6Value.TAG_OBJ && partsVal.ref() instanceof
                                                 V6Array) {
      V6Array parts = (V6Array)partsVal.ref();
      for (int i = 0; i < parts.elemCount; i++) {
        byte[] b = partBytes(parts.elements[i]);
        bos.write(b, 0, b.length);
      }
    }
    return bos.toByteArray();
  }

  static String extractType(V6Value optionsVal) {
    if (optionsVal.tag() == V6Value.TAG_OBJ && optionsVal.ref() instanceof
                                                   V6Object) {
      V6Value t = ((V6Object)optionsVal.ref()).get("type");
      if (t.tag() == V6Value.TAG_STR)
        return t.toString();
    }
    return "";
  }

  @Override
  public V6Object allocate() {
    return new V6BlobObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6BlobObject blob = new V6BlobObject();
    blob.setProto(PROTOTYPE);
    initInstance(blob, args);
    return objValue(blob);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6BlobObject blob = (V6BlobObject)instance;
    blob.data = concatParts(V6Value.argAt(args, 0));
    blob.type = extractType(V6Value.argAt(args, 1));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  static V6BlobObject self(V6Value t) {
    return (V6BlobObject)t.ref();
  }

  static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.defineGetter("size", (t, a) -> num(self(t).data.length));
    o.defineGetter("type", (t, a) -> str(self(t).type));

    o.set("slice", fn((t, a) -> {
            V6BlobObject b = self(t);
            int len = b.data.length;
            int start =
                a.length > 0 ? normalizeIndex((int)a[0].toNumber(), len) : 0;
            int end = a.length > 1 && !a[1].isUndefined()
                          ? normalizeIndex((int)a[1].toNumber(), len)
                          : len;
            if (end < start)
              end = start;
            String contentType = a.length > 2 && a[2].tag() == V6Value.TAG_STR
                                     ? a[2].toString()
                                     : "";
            V6BlobObject sliced = new V6BlobObject();
            sliced.setProto(PROTOTYPE);
            sliced.data = java.util.Arrays.copyOfRange(b.data, start, end);
            sliced.type = contentType;
            return objValue(sliced);
          }));

    o.set("arrayBuffer",
          fn((t, a)
                 -> objValue(V6Promise.resolved(
                     V6ArrayBufferConstructor.wrap(self(t).data)))));

    o.set("bytes", fn((t, a)
                          -> objValue(V6Promise.resolved(
                              objValue(new V6Buffer(self(t).data))))));

    o.set("text", fn((t, a)
                         -> objValue(V6Promise.resolved(str(new String(
                             self(t).data, StandardCharsets.UTF_8))))));

    o.set(
        "stream", fn((t, a) -> {
          V6BlobObject b = self(t);
          V6Object src = new V6Object();
          src.set("start", fn((t2, a2) -> {
                    V6Object controller = (V6Object)V6Value.argAt(a2, 0).ref();
                    controller.get("enqueue").asCallable().call(
                        V6Value.argAt(a2, 0),
                        new V6Value[] {objValue(new V6Buffer(b.data))});
                    controller.get("close").asCallable().call(
                        V6Value.argAt(a2, 0), new V6Value[0]);
                    return UNDEF;
                  }));
          return objValue(V6ReadableStreamConstructor.newStream(objValue(src)));
        }));

    return o;
  }

  private static int normalizeIndex(int idx, int len) {
    if (idx < 0)
      idx = Math.max(0, len + idx);
    return Math.min(idx, len);
  }
}

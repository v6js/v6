import java.nio.charset.StandardCharsets;

public final class V6TextEncoderConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6TextEncoderConstructor() {
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

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object self = new V6Object();
    self.setProto(PROTOTYPE);
    return objValue(self);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.defineGetter("encoding", (t, a) -> str("utf-8"));

    o.set("encode", fn((t, a) -> {
            String s =
                a.length > 0 && !a[0].isUndefined() ? a[0].toString() : "";
            return objValue(new V6Buffer(s.getBytes(StandardCharsets.UTF_8)));
          }));

    o.set("encodeInto", fn((t, a) -> {
            String s = V6Value.argAt(a, 0).toString();
            V6Value destVal = V6Value.argAt(a, 1);
            byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
            V6Object result = new V6Object();
            if (destVal.tag() == V6Value.TAG_OBJ && destVal.ref() instanceof
                                                        V6Buffer) {
              V6Buffer dest = (V6Buffer)destVal.ref();
              int destLen = (int)dest.get("length").num();
              int written = Math.min(bytes.length, destLen);
              for (int i = 0; i < written; i++)
                dest.set(Integer.toString(i), num(bytes[i] & 0xFF));
              result.set("read",
                         num(written == bytes.length ? s.length() : written));
              result.set("written", num(written));
            } else {
              result.set("read", num(0));
              result.set("written", num(0));
            }
            return objValue(result);
          }));

    return o;
  }
}

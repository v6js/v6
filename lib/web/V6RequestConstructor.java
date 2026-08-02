import java.nio.charset.StandardCharsets;

public final class V6RequestConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6RequestConstructor() {
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

  @Override
  public V6Object allocate() {
    return new V6RequestObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6RequestObject req = new V6RequestObject();
    req.setProto(PROTOTYPE);
    initInstance(req, args);
    return objValue(req);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6RequestObject req = (V6RequestObject)instance;
    V6Value inputVal = V6Value.argAt(args, 0);
    V6Value initVal = V6Value.argAt(args, 1);

    if (inputVal.tag() == V6Value.TAG_OBJ && inputVal.ref() instanceof
                                                 V6RequestObject) {
      V6RequestObject src = (V6RequestObject)inputVal.ref();
      req.url = src.url;
      req.method = src.method;
      req.headers = V6HeadersConstructor.newHeaders(objValue(src.headers));
      req.bodyBytes = src.bodyBytes;
      req.signal = src.signal;
    } else {
      req.url = inputVal.toString();
      req.headers = V6HeadersConstructor.newHeaders(UNDEF);
    }

    if (initVal.tag() == V6Value.TAG_OBJ && initVal.ref() instanceof V6Object) {
      V6Object init = (V6Object)initVal.ref();
      V6Value methodVal = init.get("method");
      if (!methodVal.isUndefined())
        req.method = methodVal.toString().toUpperCase();
      V6Value headersVal = init.get("headers");
      if (!headersVal.isUndefined())
        req.headers = V6HeadersConstructor.newHeaders(headersVal);
      V6Value signalVal = init.get("signal");
      if (!signalVal.isUndefined())
        req.signal = signalVal;
      V6Value bodyVal = init.get("body");
      if (!bodyVal.isUndefined()) {
        Object[] normalized = V6BodyUtil.normalize(bodyVal);
        req.bodyBytes = (byte[])normalized[0];
        String contentType = (String)normalized[1];
        if (contentType != null && req.headers.map.get("content-type") == null)
          req.headers.map.put("content-type", contentType);
      }
    }
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6RequestObject self(V6Value t) {
    return (V6RequestObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.defineGetter("url", (t, a) -> str(self(t).url));
    o.defineGetter("method", (t, a) -> str(self(t).method));
    o.defineGetter("headers", (t, a) -> objValue(self(t).headers));
    o.defineGetter("signal", (t, a) -> self(t).signal);
    o.defineGetter(
        "bodyUsed",
        (t,
         a) -> new V6Value(V6Value.TAG_BOOL, self(t).bodyUsed ? 1 : 0, null));

    o.set("text", fn((t, a) -> {
            V6RequestObject r = self(t);
            r.bodyUsed = true;
            return objValue(V6Promise.resolved(
                str(new String(r.bodyBytes, StandardCharsets.UTF_8))));
          }));

    o.set("json", fn((t, a) -> {
            V6RequestObject r = self(t);
            r.bodyUsed = true;
            String text = new String(r.bodyBytes, StandardCharsets.UTF_8);
            try {
              return objValue(V6Promise.resolved(V6Json.parse(text)));
            } catch (RuntimeException e) {
              return objValue(
                  V6Promise.rejected(str("SyntaxError: invalid JSON")));
            }
          }));

    o.set("arrayBuffer", fn((t, a) -> {
            V6RequestObject r = self(t);
            r.bodyUsed = true;
            return objValue(
                V6Promise.resolved(V6ArrayBufferConstructor.wrap(r.bodyBytes)));
          }));

    o.set("blob", fn((t, a) -> {
            V6RequestObject r = self(t);
            r.bodyUsed = true;
            V6BlobObject blob = new V6BlobObject();
            blob.setProto(V6BlobConstructor.PROTOTYPE);
            blob.data = r.bodyBytes;
            String ct = r.headers.map.get("content-type");
            blob.type = ct == null ? "" : ct;
            return objValue(V6Promise.resolved(objValue(blob)));
          }));

    o.set("clone", fn((t, a) -> {
            V6RequestObject r = self(t);
            V6RequestObject copy = new V6RequestObject();
            copy.setProto(PROTOTYPE);
            copy.url = r.url;
            copy.method = r.method;
            copy.headers = V6HeadersConstructor.newHeaders(objValue(r.headers));
            copy.bodyBytes = r.bodyBytes;
            copy.signal = r.signal;
            return objValue(copy);
          }));

    return o;
  }
}

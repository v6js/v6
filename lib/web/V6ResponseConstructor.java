import java.nio.charset.StandardCharsets;

public final class V6ResponseConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6ResponseConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;

    set("error", fn((thisArg, args) -> {
          V6ResponseObject r = new V6ResponseObject();
          r.setProto(PROTOTYPE);
          r.status = 0;
          r.isError = true;
          r.headers = V6HeadersConstructor.newHeaders(UNDEF);
          return objValue(r);
        }));

    set("redirect", fn((thisArg, args) -> {
          V6ResponseObject r = new V6ResponseObject();
          r.setProto(PROTOTYPE);
          r.status = args.length > 1 ? (int)args[1].toNumber() : 302;
          r.headers = V6HeadersConstructor.newHeaders(UNDEF);
          r.headers.map.put("location", V6Value.argAt(args, 0).toString());
          return objValue(r);
        }));

    set("json", fn((thisArg, args) -> {
          String json =
              V6Json.stringify(V6Value.argAt(args, 0), UNDEF, UNDEF).toString();
          V6ResponseObject r = newResponse(str(json), V6Value.argAt(args, 1));
          if (r.headers.map.get("content-type") == null)
            r.headers.map.put("content-type", "application/json");
          return objValue(r);
        }));
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

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public static V6ResponseObject newResponse(V6Value bodyVal, V6Value initVal) {
    V6ResponseObject r = new V6ResponseObject();
    r.setProto(PROTOTYPE);
    r.headers = V6HeadersConstructor.newHeaders(UNDEF);

    if (!bodyVal.isUndefined() && bodyVal.tag() != V6Value.TAG_NULL) {
      Object[] normalized = V6BodyUtil.normalize(bodyVal);
      r.bodyBytes = (byte[])normalized[0];
      String contentType = (String)normalized[1];
      if (contentType != null)
        r.headers.map.put("content-type", contentType);
    }

    if (initVal.tag() == V6Value.TAG_OBJ && initVal.ref() instanceof V6Object) {
      V6Object init = (V6Object)initVal.ref();
      V6Value statusVal = init.get("status");
      if (!statusVal.isUndefined())
        r.status = (int)statusVal.toNumber();
      V6Value statusTextVal = init.get("statusText");
      if (!statusTextVal.isUndefined())
        r.statusText = statusTextVal.toString();
      V6Value headersVal = init.get("headers");
      if (!headersVal.isUndefined()) {
        V6HeadersObject h = V6HeadersConstructor.newHeaders(headersVal);
        r.headers.map.putAll(h.map);
      }
    }
    return r;
  }

  @Override
  public V6Object allocate() {
    return new V6ResponseObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    return objValue(
        newResponse(V6Value.argAt(args, 0), V6Value.argAt(args, 1)));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6ResponseObject self(V6Value t) {
    return (V6ResponseObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.defineGetter("status", (t, a) -> num(self(t).status));
    o.defineGetter("statusText", (t, a) -> str(self(t).statusText));
    o.defineGetter(
        "ok", (t, a) -> bool(self(t).status >= 200 && self(t).status < 300));
    o.defineGetter("headers", (t, a) -> objValue(self(t).headers));
    o.defineGetter("url", (t, a) -> str(self(t).url));
    o.defineGetter("redirected", (t, a) -> bool(false));
    o.defineGetter("type", (t, a) -> str(self(t).isError ? "error" : "basic"));
    o.defineGetter("bodyUsed", (t, a) -> bool(self(t).bodyUsed));

    o.set("text", fn((t, a) -> {
            V6ResponseObject r = self(t);
            r.bodyUsed = true;
            return objValue(V6Promise.resolved(
                str(new String(r.bodyBytes, StandardCharsets.UTF_8))));
          }));

    o.set("json", fn((t, a) -> {
            V6ResponseObject r = self(t);
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
            V6ResponseObject r = self(t);
            r.bodyUsed = true;
            return objValue(
                V6Promise.resolved(V6ArrayBufferConstructor.wrap(r.bodyBytes)));
          }));

    o.set("bytes", fn((t, a) -> {
            V6ResponseObject r = self(t);
            r.bodyUsed = true;
            return objValue(
                V6Promise.resolved(objValue(new V6Buffer(r.bodyBytes))));
          }));

    o.set("blob", fn((t, a) -> {
            V6ResponseObject r = self(t);
            r.bodyUsed = true;
            V6BlobObject blob = new V6BlobObject();
            blob.setProto(V6BlobConstructor.PROTOTYPE);
            blob.data = r.bodyBytes;
            String ct = r.headers.map.get("content-type");
            blob.type = ct == null ? "" : ct;
            return objValue(V6Promise.resolved(objValue(blob)));
          }));

    o.set("clone", fn((t, a) -> {
            V6ResponseObject r = self(t);
            V6ResponseObject copy = new V6ResponseObject();
            copy.setProto(PROTOTYPE);
            copy.status = r.status;
            copy.statusText = r.statusText;
            copy.headers = V6HeadersConstructor.newHeaders(objValue(r.headers));
            copy.bodyBytes = r.bodyBytes;
            copy.url = r.url;
            copy.isError = r.isError;
            return objValue(copy);
          }));

    return o;
  }
}

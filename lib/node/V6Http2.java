import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;

public final class V6Http2 {
  private V6Http2() {
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

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Object buildConstants() {
    V6Object c = new V6Object();
    c.set("HTTP2_HEADER_PATH", str(":path"));
    c.set("HTTP2_HEADER_METHOD", str(":method"));
    c.set("HTTP2_HEADER_STATUS", str(":status"));
    c.set("HTTP2_HEADER_AUTHORITY", str(":authority"));
    c.set("HTTP2_HEADER_SCHEME", str(":scheme"));
    c.set("HTTP2_METHOD_GET", str("GET"));
    c.set("HTTP2_METHOD_POST", str("POST"));
    return c;
  }

  private static V6Object
  buildRequestStream(HttpClient client, String url, String method,
                     java.util.List<String> extraHeaders) {
    V6EventEmitterObject stream = new V6EventEmitterObject();
    stream.setProto(V6EventEmitterConstructor.PROTOTYPE);
    stream.set("end", fn((t, a) -> UNDEF));
    stream.set("close", fn((t, a) -> UNDEF));
    stream.set("setEncoding", fn((t, a) -> UNDEF));

    HttpRequest.Builder rb =
        HttpRequest.newBuilder(URI.create(url))
            .method(method, HttpRequest.BodyPublishers.noBody());
    for (int i = 0; i + 1 < extraHeaders.size(); i += 2)
      rb.header(extraHeaders.get(i), extraHeaders.get(i + 1));
    HttpRequest req = rb.build();

    V6EventLoop.ref();
    Object capturedLoop = V6EventLoop.captureState();
    client.sendAsync(req, HttpResponse.BodyHandlers.ofByteArray())
        .whenComplete((resp, err) -> {
          V6EventLoop.postExternalTo(capturedLoop, () -> {
            if (err != null) {
              stream.get("emit").asCallable().call(
                  objValue(stream),
                  new V6Value[] {str("error"),
                                 str(String.valueOf(err.getMessage()))});
            } else {
              V6Object headersOut = new V6Object();
              headersOut.set(":status", num(resp.statusCode()));
              resp.headers().map().forEach(
                  (k,
                   vList) -> headersOut.set(k, str(String.join(", ", vList))));
              stream.get("emit").asCallable().call(
                  objValue(stream),
                  new V6Value[] {str("response"), objValue(headersOut)});
              stream.get("emit").asCallable().call(
                  objValue(stream),
                  new V6Value[] {str("data"),
                                 objValue(new V6Buffer(resp.body()))});
              stream.get("emit").asCallable().call(objValue(stream),
                                                   new V6Value[] {str("end")});
            }
          });
          V6EventLoop.unref();
        });

    return stream;
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("connect", fn((thisArg, args) -> {
            String authority = V6Value.argAt(args, 0).toString();
            V6EventEmitterObject session = new V6EventEmitterObject();
            session.setProto(V6EventEmitterConstructor.PROTOTYPE);
            HttpClient client = HttpClient.newBuilder()
                                    .version(HttpClient.Version.HTTP_2)
                                    .build();

            session.set("request", fn((t, a) -> {
                          V6Value headersVal = V6Value.argAt(a, 0);
                          String path = "/";
                          String method = "GET";
                          java.util.List<String> extraHeaders =
                              new java.util.ArrayList<>();
                          if (headersVal.tag() == V6Value.TAG_OBJ &&
                              headersVal.ref() instanceof V6Object) {
                            V6Object h = (V6Object)headersVal.ref();
                            for (String k : h.keySet()) {
                              String v = h.get(k).toString();
                              if (k.equals(":path"))
                                path = v;
                              else if (k.equals(":method"))
                                method = v;
                              else if (!k.startsWith(":")) {
                                extraHeaders.add(k);
                                extraHeaders.add(v);
                              }
                            }
                          }
                          return objValue(buildRequestStream(
                              client, authority + path, method, extraHeaders));
                        }));

            session.set(
                "close", fn((t, a) -> {
                  V6Callable cb = a.length > 0 && a[0].tag() == V6Value.TAG_FUNC
                                      ? a[0].asCallable()
                                      : null;
                  if (cb != null)
                    V6MicrotaskQueue.enqueue(
                        () -> cb.call(UNDEF, new V6Value[0]));
                  V6MicrotaskQueue.enqueue(
                      ()
                          -> session.get("emit").asCallable().call(
                              objValue(session), new V6Value[] {str("close")}));
                  return UNDEF;
                }));
            session.set("destroy", fn((t, a) -> UNDEF));

            V6MicrotaskQueue.enqueue(
                ()
                    -> session.get("emit").asCallable().call(
                        objValue(session), new V6Value[] {str("connect")}));

            return objValue(session);
          }));

    o.set("constants", objValue(buildConstants()));

    o.set("createServer", fn((thisArg, args) -> {
            throw new V6Throw(str(
                "http2.createServer is not supported (no HTTP/2 server framing "
                + "implementation; use http2.connect for client-only usage, or "
                + "http/https for servers)"));
          }));
    o.set("createSecureServer", fn((thisArg, args) -> {
            throw new V6Throw(str(
                "http2.createSecureServer is not supported (no HTTP/2 server "
                + "framing "
                + "implementation; use http2.connect for client-only usage, or "
                + "http/https for servers)"));
          }));

    return o;
  }
}

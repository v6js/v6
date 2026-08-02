import java.net.URI;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.util.concurrent.CompletableFuture;

public final class V6Fetch {
  private V6Fetch() {
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

  public static V6Value fetch(V6Value[] args) {
    V6Value reqVal = new V6RequestConstructor().construct(args);
    V6RequestObject req = (V6RequestObject)reqVal.ref();

    V6Promise promise = new V6Promise();
    try {
      HttpRequest.Builder builder = HttpRequest.newBuilder(URI.create(req.url));
      for (java.util.Map.Entry<String, String> e : req.headers.map.entrySet())
        builder.header(e.getKey(), e.getValue());
      HttpRequest.BodyPublisher pub =
          req.bodyBytes.length > 0
              ? HttpRequest.BodyPublishers.ofByteArray(req.bodyBytes)
              : HttpRequest.BodyPublishers.noBody();
      builder.method(req.method, pub);
      HttpRequest httpReq = builder.build();

      V6EventLoop.ref();
      CompletableFuture<HttpResponse<byte[]>> future =
          V6Http.DEFAULT_CLIENT.sendAsync(
              httpReq, HttpResponse.BodyHandlers.ofByteArray());

      if (req.signal.tag() == V6Value.TAG_OBJ && req.signal.ref() instanceof
                                                     V6AbortSignalObject) {
        V6AbortSignalObject signal = (V6AbortSignalObject)req.signal.ref();
        if (signal.aborted) {
          future.cancel(true);
          promise.reject(signal.reason);
        } else {
          signal.addListener("abort", fn((t, a) -> {
                               future.cancel(true);
                               promise.reject(signal.reason);
                               return UNDEF;
                             }),
                             true, null);
        }
      }

      future.whenComplete((resp, err) -> {
        V6EventLoop.postExternal(() -> {
          if (err == null) {
            V6ResponseObject response = new V6ResponseObject();
            response.setProto(V6ResponseConstructor.PROTOTYPE);
            response.status = resp.statusCode();
            response.statusText = "";
            response.url = req.url;
            response.bodyBytes = resp.body();
            V6HeadersObject respHeaders =
                V6HeadersConstructor.newHeaders(UNDEF);
            resp.headers().map().forEach(
                (k, v)
                    -> respHeaders.map.put(k.toLowerCase(),
                                           String.join(", ", v)));
            response.headers = respHeaders;
            promise.resolve(objValue(response));
          } else if (!(err.getCause() instanceof
                       java.util.concurrent.CancellationException) &&
                     !(err instanceof
                       java.util.concurrent.CancellationException)) {
            promise.reject(str("TypeError: fetch failed: " + err.getMessage()));
          }
          V6EventLoop.unref();
        });
      });
    } catch (Exception e) {
      promise.reject(str("TypeError: " + e.getMessage()));
    }

    return objValue(promise);
  }
}

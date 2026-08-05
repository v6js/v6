import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;

public final class V6Http {
  private V6Http() {
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

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  private static java.util.Set<String> keysOf(V6Object o) {
    java.util.Set<String> keys = new java.util.HashSet<>();
    for (int i = 0; i < o.elemCount; i++)
      keys.add(Integer.toString(i));
    keys.addAll(o.keySet());
    return keys;
  }

  private static void
  handleHttpExchange(com.sun.net.httpserver.HttpExchange exchange,
                     V6EventEmitterObject server, CountDownLatch latch)
      throws IOException {
    V6EventEmitterObject req = new V6EventEmitterObject();
    req.setProto(V6EventEmitterConstructor.PROTOTYPE);
    for (String k : keysOf(V6EventEmitterConstructor.PROTOTYPE))
      req.set(k, V6EventEmitterConstructor.PROTOTYPE.get(k));
    req.set("method", str(exchange.getRequestMethod()));
    req.set("url", str(exchange.getRequestURI().toString()));
    V6Object reqHeaders = new V6Object();
    for (Map.Entry<String, java.util.List<String>> e :
         exchange.getRequestHeaders().entrySet())
      reqHeaders.set(e.getKey().toLowerCase(),
                     str(String.join(", ", e.getValue())));
    req.set("headers", objValue(reqHeaders));
    req.set("httpVersionMajor", num(1));
    req.set("httpVersionMinor", num(1));
    req.set("unpipe", fn((t, a) -> t));
    req.set("resume", fn((t, a) -> t));
    req.set("pause", fn((t, a) -> t));
    req.set("destroy", fn((t, a) -> t));
    req.set("pipe", fn((t, a) -> V6Value.argAt(a, 0)));
    byte[] bodyBytes = exchange.getRequestBody().readAllBytes();

    V6EventEmitterObject res = new V6EventEmitterObject();
    res.setProto(V6EventEmitterConstructor.PROTOTYPE);
    for (String k : keysOf(V6EventEmitterConstructor.PROTOTYPE))
      res.set(k, V6EventEmitterConstructor.PROTOTYPE.get(k));
    res.set("statusCode", num(200));
    res.set("statusMessage", str(""));
    res.set("headersSent", V6Value.FALSE);
    Map<String, String> resHeaders = new LinkedHashMap<>();
    boolean[] ended = {false};
    boolean[] headersSent = {false};
    java.io.OutputStream[] responseStreamHolder = new java.io.OutputStream[1];

    res.set("setHeader", fn((t, a) -> {
              resHeaders.put(V6Value.argAt(a, 0).toString(),
                             V6Value.argAt(a, 1).toString());
              return UNDEF;
            }));
    res.set("getHeader", fn((t, a) -> {
              String v = resHeaders.get(V6Value.argAt(a, 0).toString());
              return v != null ? str(v) : UNDEF;
            }));
    res.set("removeHeader", fn((t, a) -> {
              resHeaders.remove(V6Value.argAt(a, 0).toString());
              return UNDEF;
            }));
    res.set("writeHead", fn((t, a) -> {
              ((V6Object)t.ref())
                  .set("statusCode", num(V6Value.argAt(a, 0).toNumber()));
              for (V6Value v : a)
                if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Object &&
                    !(v.ref() instanceof V6Array)) {
                  V6Object h = (V6Object)v.ref();
                  for (String k : keysOf(h))
                    resHeaders.put(k, h.get(k).toString());
                }
              return t;
            }));
    res.set("write", fn((t, a) -> {
              try {
                if (!headersSent[0]) {
                  for (Map.Entry<String, String> e : resHeaders.entrySet())
                    exchange.getResponseHeaders().add(e.getKey(), e.getValue());
                  exchange.sendResponseHeaders(
                      (int)((V6Object)t.ref()).get("statusCode").toNumber(), 0);
                  headersSent[0] = true;
                  ((V6Object)t.ref()).set("headersSent", V6Value.TRUE);
                  responseStreamHolder[0] = exchange.getResponseBody();
                }
                byte[] bytes = bytesOf(V6Value.argAt(a, 0));
                responseStreamHolder[0].write(bytes);
                responseStreamHolder[0].flush();
              } catch (IOException ignored) {
              }
              return new V6Value(V6Value.TAG_BOOL, 1, null);
            }));
    res.set("end", fn((t, a) -> {
              try {
                if (!headersSent[0]) {
                  byte[] finalBytes =
                      a.length > 0 && a[0].tag() != V6Value.TAG_FUNC
                          ? bytesOf(a[0])
                          : new byte[0];
                  for (Map.Entry<String, String> e : resHeaders.entrySet())
                    exchange.getResponseHeaders().add(e.getKey(), e.getValue());
                  exchange.sendResponseHeaders(
                      (int)((V6Object)t.ref()).get("statusCode").toNumber(),
                      finalBytes.length);
                  responseStreamHolder[0] = exchange.getResponseBody();
                  if (finalBytes.length > 0)
                    responseStreamHolder[0].write(finalBytes);
                  headersSent[0] = true;
                  ((V6Object)t.ref()).set("headersSent", V6Value.TRUE);
                } else if (a.length > 0 && a[0].tag() != V6Value.TAG_FUNC) {
                  byte[] bytes = bytesOf(a[0]);
                  responseStreamHolder[0].write(bytes);
                }
                responseStreamHolder[0].close();
              } catch (IOException ignored) {
              } finally {
                exchange.close();
                ended[0] = true;
                latch.countDown();
              }
              V6Callable cb =
                  a.length > 0 && a[a.length - 1].tag() == V6Value.TAG_FUNC
                      ? a[a.length - 1].asCallable()
                      : null;
              if (cb != null)
                cb.call(UNDEF, new V6Value[0]);
              return UNDEF;
            }));

    server.get("emit").asCallable().call(
        objValue(server),
        new V6Value[] {str("request"), objValue(req), objValue(res)});

    if (bodyBytes.length > 0)
      req.get("emit").asCallable().call(
          objValue(req),
          new V6Value[] {str("data"), objValue(new V6Buffer(bodyBytes))});
    req.get("emit").asCallable().call(objValue(req),
                                      new V6Value[] {str("end")});

    if (!ended[0]) {
      try {
        exchange.sendResponseHeaders(200, -1);
      } catch (IOException ignored) {
      } finally {
        exchange.close();
      }
    }
  }

  private static byte[] pemBytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    if (v.isUndefined())
      return new byte[0];
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  private static V6Object buildCreateServerModule(boolean isHttps) {
    V6Object o = new V6Object();
    o.set(
        "createServer", fn((thisArg, args) -> {
          V6Callable requestListener = null;
          V6Object tlsOptions = null;
          for (V6Value a : args) {
            if (a.tag() == V6Value.TAG_FUNC)
              requestListener = a.asCallable();
            else if (a.tag() == V6Value.TAG_OBJ)
              tlsOptions = (V6Object)a.ref();
          }
          final V6Callable fReqListener = requestListener;

          javax.net.ssl.SSLContext sslCtx = null;
          if (isHttps) {
            if (tlsOptions == null)
              throw new V6Throw(str("https.createServer requires { key, "
                                    + "cert } options (PEM-encoded)"));
            try {
              sslCtx = V6TlsUtil.buildServerContext(
                  pemBytesOf(tlsOptions.get("key")),
                  pemBytesOf(tlsOptions.get("cert")));
            } catch (Exception e) {
              throw new V6Throw(
                  str("https.createServer: failed to load TLS key/cert: " +
                      e.getMessage()));
            }
          }
          final javax.net.ssl.SSLContext fSslCtx = sslCtx;

          V6EventEmitterObject server = new V6EventEmitterObject();
          server.setProto(V6EventEmitterConstructor.PROTOTYPE);
          if (fReqListener != null)
            server.get("on").asCallable().call(
                objValue(server),
                new V6Value[] {str("request"), fn(fReqListener)});

          com.sun.net.httpserver.HttpServer[] httpServerHolder =
              new com.sun.net.httpserver.HttpServer[1];

          server.set(
              "listen", fn((t, a) -> {
                int port = (int)V6Value.argAt(a, 0).toNumber();
                V6Callable listenCb = null;
                for (V6Value v : a)
                  if (v.tag() == V6Value.TAG_FUNC)
                    listenCb = v.asCallable();
                final V6Callable fListenCb = listenCb;
                try {
                  com.sun.net.httpserver.HttpServer hs;
                  if (fSslCtx != null) {
                    com.sun.net.httpserver.HttpsServer hss =
                        com.sun.net.httpserver.HttpsServer.create(
                            new InetSocketAddress(port), 0);
                    hss.setHttpsConfigurator(
                        new com.sun.net.httpserver.HttpsConfigurator(fSslCtx));
                    hs = hss;
                  } else {
                    hs = com.sun.net.httpserver.HttpServer.create(
                        new InetSocketAddress(port), 0);
                  }
                  httpServerHolder[0] = hs;
                  hs.createContext("/", exchange -> {
                    CountDownLatch latch = new CountDownLatch(1);
                    V6EventLoop.postExternal(() -> {
                      try {
                        handleHttpExchange(exchange, server, latch);
                      } catch (IOException | RuntimeException e) {
                        latch.countDown();
                      }
                    });
                    try {
                      latch.await();
                    } catch (InterruptedException ignored) {
                    }
                  });
                  hs.setExecutor(Executors.newCachedThreadPool(r -> {
                    Thread th = new Thread(r);
                    th.setDaemon(true);
                    return th;
                  }));
                  hs.start();
                  V6EventLoop.ref();
                  if (fListenCb != null)
                    V6MicrotaskQueue.enqueue(
                        () -> fListenCb.call(UNDEF, new V6Value[0]));
                  V6MicrotaskQueue.enqueue(
                      ()
                          -> server.get("emit").asCallable().call(
                              objValue(server),
                              new V6Value[] {str("listening")}));
                } catch (IOException e) {
                  V6MicrotaskQueue.enqueue(
                      ()
                          -> server.get("emit").asCallable().call(
                              objValue(server),
                              new V6Value[] {
                                  str("error"),
                                  str(String.valueOf(e.getMessage()))}));
                }
                return t;
              }));

          server.set("close", fn((t, a) -> {
                       if (httpServerHolder[0] != null) {
                         httpServerHolder[0].stop(0);
                         V6EventLoop.unref();
                       }
                       V6Callable cb =
                           a.length > 0 && a[0].tag() == V6Value.TAG_FUNC
                               ? a[0].asCallable()
                               : null;
                       server.get("emit").asCallable().call(
                           objValue(server), new V6Value[] {str("close")});
                       if (cb != null)
                         cb.call(UNDEF, new V6Value[0]);
                       return t;
                     }));

          server.set(
              "address", fn((t, a) -> {
                if (httpServerHolder[0] == null)
                  return new V6Value(V6Value.TAG_NULL, 0, null);
                InetSocketAddress addr = httpServerHolder[0].getAddress();
                V6Object result = new V6Object();
                result.set("port", num(addr.getPort()));
                result.set("address", str(addr.getAddress().getHostAddress()));
                result.set("family", str(addr.getAddress() instanceof
                                                 java.net.Inet6Address
                                             ? "IPv6"
                                             : "IPv4"));
                return objValue(result);
              }));

          return objValue(server);
        }));
    return o;
  }

  static final HttpClient DEFAULT_CLIENT = HttpClient.newHttpClient();

  private static void sendHttpRequest(String urlStr, String method,
                                      Map<String, String> headers, byte[] body,
                                      V6EventEmitterObject reqObj,
                                      V6Callable responseCb,
                                      boolean rejectUnauthorized) {
    try {
      HttpRequest.Builder builder = HttpRequest.newBuilder(URI.create(urlStr));
      for (Map.Entry<String, String> e : headers.entrySet())
        builder.header(e.getKey(), e.getValue());
      HttpRequest.BodyPublisher pub =
          body.length > 0 ? HttpRequest.BodyPublishers.ofByteArray(body)
                          : HttpRequest.BodyPublishers.noBody();
      builder.method(method, pub);
      HttpRequest req = builder.build();
      HttpClient client = DEFAULT_CLIENT;
      if (urlStr.startsWith("https:") && !rejectUnauthorized) {
        try {
          client = HttpClient.newBuilder()
                       .sslContext(V6TlsUtil.buildClientContext(false))
                       .build();
        } catch (Exception ignored) {
        }
      }
      V6EventLoop.ref();
      client.sendAsync(req, HttpResponse.BodyHandlers.ofByteArray())
          .whenComplete((resp, err) -> {
            V6EventLoop.postExternal(() -> {
              if (err != null) {
                reqObj.get("emit").asCallable().call(
                    objValue(reqObj),
                    new V6Value[] {str("error"),
                                   str(String.valueOf(err.getMessage()))});
              } else {
                V6EventEmitterObject resObj = new V6EventEmitterObject();
                resObj.setProto(V6EventEmitterConstructor.PROTOTYPE);
                resObj.set("statusCode", num(resp.statusCode()));
                V6Object headersOut = new V6Object();
                resp.headers().map().forEach(
                    (k, v)
                        -> headersOut.set(k.toLowerCase(),
                                          str(String.join(", ", v))));
                resObj.set("headers", objValue(headersOut));
                if (responseCb != null)
                  responseCb.call(UNDEF, new V6Value[] {objValue(resObj)});
                resObj.get("emit").asCallable().call(
                    objValue(resObj),
                    new V6Value[] {str("data"),
                                   objValue(new V6Buffer(resp.body()))});
                resObj.get("emit").asCallable().call(
                    objValue(resObj), new V6Value[] {str("end")});
              }
              V6EventLoop.unref();
            });
          });
    } catch (Exception e) {
      V6EventLoop.postExternal(
          ()
              -> reqObj.get("emit").asCallable().call(
                  objValue(reqObj),
                  new V6Value[] {str("error"),
                                 str(String.valueOf(e.getMessage()))}));
    }
  }

  private static V6Value requestImpl(V6Value[] args, boolean autoEnd,
                                     boolean isHttps) {
    String urlStr;
    String method = "GET";
    Map<String, String> headers = new LinkedHashMap<>();
    V6Callable responseCb = null;
    boolean rejectUnauthorized = true;
    V6Value firstArg = V6Value.argAt(args, 0);
    String defaultProtocol = isHttps ? "https:" : "http:";

    if (firstArg.tag() == V6Value.TAG_STR) {
      urlStr = firstArg.toString();
    } else if (firstArg.tag() == V6Value.TAG_OBJ && firstArg.ref() instanceof
                                                        V6UrlObject) {
      urlStr = ((V6UrlObject)firstArg.ref()).href();
    } else if (firstArg.tag() == V6Value.TAG_OBJ) {
      V6Object opts = (V6Object)firstArg.ref();
      String protocol = opts.get("protocol").isUndefined()
                            ? defaultProtocol
                            : opts.get("protocol").toString();
      String host = !opts.get("host").isUndefined()
                        ? opts.get("host").toString()
                        : (!opts.get("hostname").isUndefined()
                               ? opts.get("hostname").toString()
                               : "localhost");
      String path =
          opts.get("path").isUndefined() ? "/" : opts.get("path").toString();
      V6Value portVal = opts.get("port");
      String portPart =
          portVal.isUndefined() ? "" : ":" + (int)portVal.toNumber();
      urlStr = protocol + "//" + host + portPart + path;
      if (!opts.get("method").isUndefined())
        method = opts.get("method").toString();
      if (!opts.get("rejectUnauthorized").isUndefined())
        rejectUnauthorized = opts.get("rejectUnauthorized").truthy();
      V6Value headersVal = opts.get("headers");
      if (headersVal.tag() == V6Value.TAG_OBJ) {
        V6Object h = (V6Object)headersVal.ref();
        for (String k : keysOf(h))
          headers.put(k, h.get(k).toString());
      }
    } else {
      urlStr = defaultProtocol + "//localhost/";
    }

    for (V6Value a : args)
      if (a.tag() == V6Value.TAG_FUNC)
        responseCb = a.asCallable();

    final V6Callable fResponseCb = responseCb;
    final String fMethod = method;
    final String fUrlStr = urlStr;
    final boolean fRejectUnauthorized = rejectUnauthorized;

    V6EventEmitterObject reqObj = new V6EventEmitterObject();
    reqObj.setProto(V6EventEmitterConstructor.PROTOTYPE);
    java.io.ByteArrayOutputStream bodyBuf = new java.io.ByteArrayOutputStream();

    reqObj.set("write", fn((t, a) -> {
                 byte[] b = bytesOf(V6Value.argAt(a, 0));
                 bodyBuf.write(b, 0, b.length);
                 return new V6Value(V6Value.TAG_BOOL, 1, null);
               }));
    reqObj.set("setHeader", fn((t, a) -> {
                 headers.put(V6Value.argAt(a, 0).toString(),
                             V6Value.argAt(a, 1).toString());
                 return t;
               }));
    reqObj.set("end", fn((t, a) -> {
                 if (a.length > 0 && a[0].tag() != V6Value.TAG_FUNC) {
                   byte[] b = bytesOf(a[0]);
                   bodyBuf.write(b, 0, b.length);
                 }
                 sendHttpRequest(fUrlStr, fMethod, headers,
                                 bodyBuf.toByteArray(), reqObj, fResponseCb,
                                 fRejectUnauthorized);
                 return UNDEF;
               }));
    reqObj.set("abort", fn((t, a) -> UNDEF));

    if (autoEnd)
      sendHttpRequest(urlStr, "GET", headers, new byte[0], reqObj, fResponseCb,
                      rejectUnauthorized);

    return objValue(reqObj);
  }

  private static final String[] HTTP_METHODS = {
      "ACL",      "BIND",      "CHECKOUT",   "CONNECT",    "COPY",
      "DELETE",   "GET",       "HEAD",       "LINK",       "LOCK",
      "M-SEARCH", "MERGE",     "MKACTIVITY", "MKCALENDAR", "MKCOL",
      "MOVE",     "NOTIFY",    "OPTIONS",    "PATCH",      "POST",
      "PROPFIND", "PROPPATCH", "PURGE",      "PUT",        "QUERY",
      "REBIND",   "REPORT",    "SEARCH",     "SOURCE",     "SUBSCRIBE",
      "TRACE",    "UNBIND",    "UNLINK",     "UNLOCK",     "UNSUBSCRIBE",
  };

  private static final String[][] HTTP_STATUS_CODES = {
      {"100", "Continue"},
      {"101", "Switching Protocols"},
      {"102", "Processing"},
      {"200", "OK"},
      {"201", "Created"},
      {"202", "Accepted"},
      {"203", "Non-Authoritative Information"},
      {"204", "No Content"},
      {"205", "Reset Content"},
      {"206", "Partial Content"},
      {"300", "Multiple Choices"},
      {"301", "Moved Permanently"},
      {"302", "Found"},
      {"303", "See Other"},
      {"304", "Not Modified"},
      {"307", "Temporary Redirect"},
      {"308", "Permanent Redirect"},
      {"400", "Bad Request"},
      {"401", "Unauthorized"},
      {"402", "Payment Required"},
      {"403", "Forbidden"},
      {"404", "Not Found"},
      {"405", "Method Not Allowed"},
      {"406", "Not Acceptable"},
      {"407", "Proxy Authentication Required"},
      {"408", "Request Timeout"},
      {"409", "Conflict"},
      {"410", "Gone"},
      {"411", "Length Required"},
      {"412", "Precondition Failed"},
      {"413", "Payload Too Large"},
      {"414", "URI Too Long"},
      {"415", "Unsupported Media Type"},
      {"416", "Range Not Satisfiable"},
      {"417", "Expectation Failed"},
      {"418", "I'm a Teapot"},
      {"422", "Unprocessable Entity"},
      {"425", "Too Early"},
      {"426", "Upgrade Required"},
      {"428", "Precondition Required"},
      {"429", "Too Many Requests"},
      {"431", "Request Header Fields Too Large"},
      {"451", "Unavailable For Legal Reasons"},
      {"500", "Internal Server Error"},
      {"501", "Not Implemented"},
      {"502", "Bad Gateway"},
      {"503", "Service Unavailable"},
      {"504", "Gateway Timeout"},
      {"505", "HTTP Version Not Supported"},
  };

  public static V6Object build() {
    V6Object o = buildCreateServerModule(false);
    o.set("request", fn((thisArg, args) -> requestImpl(args, false, false)));
    o.set("get", fn((thisArg, args) -> requestImpl(args, true, false)));
    V6HttpAgentConstructor agentCtor = new V6HttpAgentConstructor();
    o.set("Agent", objValue(agentCtor));
    o.set("globalAgent", agentCtor.construct(new V6Value[0]));
    V6Array methods = new V6Array();
    for (String m : HTTP_METHODS)
      methods.push(str(m));
    o.set("METHODS", objValue(methods));
    V6Object statusCodes = new V6Object();
    for (String[] sc : HTTP_STATUS_CODES)
      statusCodes.set(sc[0], str(sc[1]));
    o.set("STATUS_CODES", objValue(statusCodes));
    return o;
  }

  public static V6Object buildHttps() {
    V6Object o = buildCreateServerModule(true);
    o.set("request", fn((thisArg, args) -> requestImpl(args, false, true)));
    o.set("get", fn((thisArg, args) -> requestImpl(args, true, true)));
    V6HttpAgentConstructor agentCtor = new V6HttpAgentConstructor();
    o.set("Agent", objValue(agentCtor));
    o.set("globalAgent", agentCtor.construct(new V6Value[0]));
    return o;
  }
}

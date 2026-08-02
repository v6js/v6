import java.io.IOException;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLServerSocket;
import javax.net.ssl.SSLServerSocketFactory;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

public final class V6Tls {
  private V6Tls() {
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

  private static byte[] pemBytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    if (v.isUndefined())
      return new byte[0];
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set(
        "createServer", fn((thisArg, args) -> {
          V6Object options = null;
          V6Callable connListener = null;
          for (V6Value a : args) {
            if (a.tag() == V6Value.TAG_FUNC)
              connListener = a.asCallable();
            else if (a.tag() == V6Value.TAG_OBJ)
              options = (V6Object)a.ref();
          }
          if (options == null)
            throw new V6Throw(
                str("tls.createServer requires { key, cert } options"));
          final V6Callable fConnListener = connListener;

          SSLContext ctx;
          try {
            ctx = V6TlsUtil.buildServerContext(pemBytesOf(options.get("key")),
                                               pemBytesOf(options.get("cert")));
          } catch (Exception e) {
            throw new V6Throw(
                str("tls.createServer: failed to load key/cert: " +
                    e.getMessage()));
          }
          SSLServerSocketFactory factory = ctx.getServerSocketFactory();

          V6EventEmitterObject server = new V6EventEmitterObject();
          server.setProto(V6EventEmitterConstructor.PROTOTYPE);
          if (fConnListener != null)
            server.get("on").asCallable().call(
                objValue(server),
                new V6Value[] {str("secureConnection"), fn(fConnListener)});

          SSLServerSocket[] serverSocketHolder = new SSLServerSocket[1];
          boolean[] closed = {false};

          server.set("listen", fn((t, a) -> {
                       int port = (int)V6Value.argAt(a, 0).toNumber();
                       V6Callable listenCb = null;
                       for (V6Value v : a)
                         if (v.tag() == V6Value.TAG_FUNC)
                           listenCb = v.asCallable();
                       final V6Callable fListenCb = listenCb;
                       try {
                         SSLServerSocket ss =
                             (SSLServerSocket)factory.createServerSocket(port);
                         serverSocketHolder[0] = ss;
                         V6EventLoop.ref();
                         Thread th = new Thread(() -> {
                           try {
                             while (!closed[0]) {
                               Socket sock = ss.accept();
                               V6EventLoop.postExternal(() -> {
                                 V6EventEmitterObject wrapped =
                                     V6Net.wrapAcceptedSocket(sock);
                                 server.get("emit").asCallable().call(
                                     objValue(server),
                                     new V6Value[] {str("secureConnection"),
                                                    objValue(wrapped)});
                               });
                             }
                           } catch (IOException ignored) {
                           } finally {
                             V6EventLoop.unref();
                           }
                         });
                         th.setDaemon(true);
                         th.start();
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
                       closed[0] = true;
                       try {
                         if (serverSocketHolder[0] != null)
                           serverSocketHolder[0].close();
                       } catch (IOException ignored) {
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

          return objValue(server);
        }));

    o.set(
        "connect", fn((thisArg, args) -> {
          V6Object options = null;
          int port = 0;
          String host = "localhost";
          V6Callable connectCb = null;
          if (args.length > 0 && args[0].tag() == V6Value.TAG_NUM) {
            port = (int)args[0].toNumber();
            if (args.length > 1 && args[1].tag() == V6Value.TAG_STR)
              host = args[1].toString();
          }
          for (V6Value a : args) {
            if (a.tag() == V6Value.TAG_FUNC)
              connectCb = a.asCallable();
            else if (a.tag() == V6Value.TAG_OBJ)
              options = (V6Object)a.ref();
          }
          if (options != null) {
            if (!options.get("port").isUndefined())
              port = (int)options.get("port").toNumber();
            if (!options.get("host").isUndefined())
              host = options.get("host").toString();
          }
          boolean rejectUnauthorized =
              options == null ||
              options.get("rejectUnauthorized").isUndefined() ||
              options.get("rejectUnauthorized").truthy();
          final int fPort = port;
          final String fHost = host;
          final V6Callable fConnectCb = connectCb;

          V6EventEmitterObject s = new V6EventEmitterObject();
          s.setProto(V6EventEmitterConstructor.PROTOTYPE);
          if (fConnectCb != null)
            s.get("on").asCallable().call(
                objValue(s),
                new V6Value[] {str("secureConnect"), fn(fConnectCb)});
          s.set("write", fn((t, a) -> {
                  throw new V6Throw(str("tls: socket not yet connected"));
                }));
          s.set("destroy", fn((t, a) -> UNDEF));

          V6EventLoop.ref();
          Thread th = new Thread(() -> {
            try {
              SSLContext ctx = V6TlsUtil.buildClientContext(rejectUnauthorized);
              SSLSocketFactory factory = ctx.getSocketFactory();
              SSLSocket sock = (SSLSocket)factory.createSocket(fHost, fPort);
              sock.startHandshake();
              V6Net.wireSocket(sock, s);
              V6EventLoop.postExternal(
                  ()
                      -> s.get("emit").asCallable().call(
                          objValue(s), new V6Value[] {str("secureConnect")}));
            } catch (Exception e) {
              V6EventLoop.postExternal(
                  ()
                      -> s.get("emit").asCallable().call(
                          objValue(s),
                          new V6Value[] {str("error"),
                                         str(String.valueOf(e.getMessage()))}));
            } finally {
              V6EventLoop.unref();
            }
          });
          th.setDaemon(true);
          th.start();

          return objValue(s);
        }));

    return o;
  }
}

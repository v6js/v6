import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public final class V6Net {
  private V6Net() {}

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
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  private static void wireSocket(Socket sock, V6EventEmitterObject s) {
    s.set("remoteAddress",
          str(sock.getInetAddress() != null ? sock.getInetAddress().getHostAddress() : ""));
    s.set("remotePort", new V6Value(V6Value.TAG_NUM, sock.getPort(), null));

    s.set("write", fn((t, a) -> {
            try {
              sock.getOutputStream().write(bytesOf(V6Value.argAt(a, 0)));
              sock.getOutputStream().flush();
            } catch (IOException e) {
              s.get("emit").asCallable().call(
                  objValue(s),
                  new V6Value[] {str("error"), str(String.valueOf(e.getMessage()))});
            }
            V6Callable cb = a.length > 1 && a[a.length - 1].tag() == V6Value.TAG_FUNC
                ? a[a.length - 1].asCallable()
                : null;
            if (cb != null)
              cb.call(UNDEF, new V6Value[0]);
            return new V6Value(V6Value.TAG_BOOL, 1, null);
          }));

    s.set("end", fn((t, a) -> {
            if (a.length > 0 && a[0].tag() != V6Value.TAG_FUNC)
              s.get("write").asCallable().call(objValue(s), new V6Value[] {a[0]});
            try {
              sock.shutdownOutput();
            } catch (IOException ignored) {
            }
            return UNDEF;
          }));

    s.set("destroy", fn((t, a) -> {
            try {
              sock.close();
            } catch (IOException ignored) {
            }
            return UNDEF;
          }));

    V6EventLoop.ref();
    Thread th = new Thread(() -> {
      try {
        java.io.InputStream in = sock.getInputStream();
        byte[] buf = new byte[8192];
        int n;
        while ((n = in.read(buf)) != -1) {
          byte[] chunk = java.util.Arrays.copyOf(buf, n);
          V6EventLoop.postExternal(
              ()
                  -> s.get("emit").asCallable().call(
                      objValue(s),
                      new V6Value[] {str("data"), objValue(new V6Buffer(chunk))}));
        }
      } catch (IOException ignored) {
      } finally {
        V6EventLoop.postExternal(() -> {
          s.get("emit").asCallable().call(objValue(s), new V6Value[] {str("end")});
          s.get("emit").asCallable().call(objValue(s), new V6Value[] {str("close")});
        });
        V6EventLoop.unref();
      }
    });
    th.setDaemon(true);
    th.start();
  }

  private static V6EventEmitterObject wrapAcceptedSocket(Socket sock) {
    V6EventEmitterObject s = new V6EventEmitterObject();
    s.setProto(V6EventEmitterConstructor.PROTOTYPE);
    wireSocket(sock, s);
    return s;
  }

  private static V6Value connectImpl(V6Value[] args) {
    int port = (int)V6Value.argAt(args, 0).toNumber();
    String host = args.length > 1 && args[1].tag() == V6Value.TAG_STR
        ? args[1].toString()
        : "localhost";
    V6Callable connectCb = null;
    for (V6Value a : args)
      if (a.tag() == V6Value.TAG_FUNC)
        connectCb = a.asCallable();
    final V6Callable fConnectCb = connectCb;

    V6EventEmitterObject s = new V6EventEmitterObject();
    s.setProto(V6EventEmitterConstructor.PROTOTYPE);
    if (fConnectCb != null)
      s.get("on").asCallable().call(objValue(s),
                                    new V6Value[] {str("connect"), fn(fConnectCb)});
    s.set("write",
          fn((t, a) -> { throw new V6Throw(str("net: socket not yet connected")); }));
    s.set("destroy", fn((t, a) -> UNDEF));

    V6EventLoop.ref();
    Thread th = new Thread(() -> {
      try {
        Socket sock = new Socket(host, port);
        wireSocket(sock, s);
        V6EventLoop.postExternal(
            () -> s.get("emit").asCallable().call(objValue(s), new V6Value[] {str("connect")}));
      } catch (IOException e) {
        V6EventLoop.postExternal(
            ()
                -> s.get("emit").asCallable().call(
                    objValue(s),
                    new V6Value[] {str("error"), str(String.valueOf(e.getMessage()))}));
      } finally {
        V6EventLoop.unref();
      }
    });
    th.setDaemon(true);
    th.start();

    return objValue(s);
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("createServer", fn((thisArg, args) -> {
            V6Callable connListener = null;
            for (V6Value a : args)
              if (a.tag() == V6Value.TAG_FUNC)
                connListener = a.asCallable();
            final V6Callable fConnListener = connListener;

            V6EventEmitterObject server = new V6EventEmitterObject();
            server.setProto(V6EventEmitterConstructor.PROTOTYPE);
            if (fConnListener != null)
              server.get("on").asCallable().call(
                  objValue(server), new V6Value[] {str("connection"), fn(fConnListener)});

            ServerSocket[] serverSocketHolder = new ServerSocket[1];
            boolean[] closed = {false};

            server.set("listen", fn((t, a) -> {
                        int port = (int)V6Value.argAt(a, 0).toNumber();
                        V6Callable listenCb = null;
                        for (V6Value v : a)
                          if (v.tag() == V6Value.TAG_FUNC)
                            listenCb = v.asCallable();
                        final V6Callable fListenCb = listenCb;
                        try {
                          ServerSocket ss = new ServerSocket(port);
                          serverSocketHolder[0] = ss;
                          V6EventLoop.ref();
                          Thread th = new Thread(() -> {
                            try {
                              while (!closed[0]) {
                                Socket sock = ss.accept();
                                V6EventLoop.postExternal(() -> {
                                  V6EventEmitterObject wrapped = wrapAcceptedSocket(sock);
                                  server.get("emit").asCallable().call(
                                      objValue(server), new V6Value[] {
                                          str("connection"), objValue(wrapped)});
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
                                      objValue(server), new V6Value[] {str("listening")}));
                        } catch (IOException e) {
                          V6MicrotaskQueue.enqueue(
                              ()
                                  -> server.get("emit").asCallable().call(
                                      objValue(server),
                                      new V6Value[] {
                                          str("error"), str(String.valueOf(e.getMessage()))}));
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
                        V6Callable cb = a.length > 0 && a[0].tag() == V6Value.TAG_FUNC
                            ? a[0].asCallable()
                            : null;
                        server.get("emit").asCallable().call(objValue(server),
                                                             new V6Value[] {str("close")});
                        if (cb != null)
                          cb.call(UNDEF, new V6Value[0]);
                        return t;
                      }));

            server.set("address", fn((t, a) -> {
                        if (serverSocketHolder[0] == null)
                          return NUL;
                        V6Object addr = new V6Object();
                        addr.set("port", new V6Value(V6Value.TAG_NUM,
                                                     serverSocketHolder[0].getLocalPort(),
                                                     null));
                        addr.set(
                            "address",
                            str(serverSocketHolder[0].getInetAddress().getHostAddress()));
                        return objValue(addr);
                      }));

            return objValue(server);
          }));

    o.set("connect", fn((thisArg, args) -> connectImpl(args)));
    o.set("createConnection", fn((thisArg, args) -> connectImpl(args)));

    return o;
  }
}

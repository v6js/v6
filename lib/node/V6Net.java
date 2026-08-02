import java.io.Closeable;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.net.StandardProtocolFamily;
import java.net.UnixDomainSocketAddress;
import java.nio.charset.StandardCharsets;
import java.nio.channels.Channels;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

public final class V6Net {
  private V6Net() {
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
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  private static void wireGeneric(InputStream in, OutputStream out,
                                  Closeable closeable, Runnable shutdownOutput,
                                  V6EventEmitterObject s, String remoteAddress,
                                  int remotePort) {
    s.set("remoteAddress", str(remoteAddress));
    s.set("remotePort", new V6Value(V6Value.TAG_NUM, remotePort, null));

    s.set("write", fn((t, a) -> {
            try {
              out.write(bytesOf(V6Value.argAt(a, 0)));
              out.flush();
            } catch (IOException e) {
              s.get("emit").asCallable().call(
                  objValue(s),
                  new V6Value[] {str("error"),
                                 str(String.valueOf(e.getMessage()))});
            }
            V6Callable cb =
                a.length > 1 && a[a.length - 1].tag() == V6Value.TAG_FUNC
                    ? a[a.length - 1].asCallable()
                    : null;
            if (cb != null)
              cb.call(UNDEF, new V6Value[0]);
            return new V6Value(V6Value.TAG_BOOL, 1, null);
          }));

    s.set("end", fn((t, a) -> {
            if (a.length > 0 && a[0].tag() != V6Value.TAG_FUNC)
              s.get("write").asCallable().call(objValue(s),
                                               new V6Value[] {a[0]});
            try {
              shutdownOutput.run();
            } catch (RuntimeException ignored) {
            }
            return UNDEF;
          }));

    s.set("destroy", fn((t, a) -> {
            try {
              closeable.close();
            } catch (IOException ignored) {
            }
            return UNDEF;
          }));

    s.set("setKeepAlive", fn((t, a) -> t));
    s.set("setNoDelay", fn((t, a) -> t));
    s.set("setTimeout", fn((t, a) -> {
            V6Callable cb = a.length > 1 && a[1].tag() == V6Value.TAG_FUNC
                                ? a[1].asCallable()
                                : null;
            if (cb != null)
              s.get("on").asCallable().call(
                  objValue(s), new V6Value[] {str("timeout"), fn(cb)});
            return t;
          }));

    V6EventLoop.ref();
    Thread th = new Thread(() -> {
      try {
        byte[] buf = new byte[8192];
        while (true) {
          int n;
          try {
            n = in.read(buf);
          } catch (SocketTimeoutException ste) {
            V6EventLoop.postExternal(
                ()
                    -> s.get("emit").asCallable().call(
                        objValue(s), new V6Value[] {str("timeout")}));
            continue;
          }
          if (n == -1)
            break;
          byte[] chunk = java.util.Arrays.copyOf(buf, n);
          V6EventLoop.postExternal(
              ()
                  -> s.get("emit").asCallable().call(
                      objValue(s),
                      new V6Value[] {str("data"),
                                     objValue(new V6Buffer(chunk))}));
        }
      } catch (IOException ignored) {
      } finally {
        V6EventLoop.postExternal(() -> {
          s.get("emit").asCallable().call(objValue(s),
                                          new V6Value[] {str("end")});
          s.get("emit").asCallable().call(objValue(s),
                                          new V6Value[] {str("close")});
        });
        V6EventLoop.unref();
      }
    });
    th.setDaemon(true);
    th.start();
  }

  static void wireSocket(Socket sock, V6EventEmitterObject s) {
    try {
      wireGeneric(sock.getInputStream(), sock.getOutputStream(), sock,
                  ()
                      -> {
                    try {
                      sock.shutdownOutput();
                    } catch (IOException ignored) {
                    }
                  },
                  s,
                  sock.getInetAddress() != null
                      ? sock.getInetAddress().getHostAddress()
                      : "",
                  sock.getPort());
    } catch (IOException e) {
      throw new V6Throw(str("net: " + e.getMessage()));
    }

    s.set("setNoDelay", fn((t, a) -> {
            try {
              sock.setTcpNoDelay(a.length == 0 || a[0].truthy());
            } catch (IOException ignored) {
            }
            return t;
          }));
    s.set("setKeepAlive", fn((t, a) -> {
            try {
              sock.setKeepAlive(a.length == 0 || a[0].truthy());
            } catch (IOException ignored) {
            }
            return t;
          }));
    V6Value existingSetTimeout = s.get("setTimeout");
    s.set("setTimeout", fn((t, a) -> {
            try {
              sock.setSoTimeout((int)V6Value.argAt(a, 0).toNumber());
            } catch (IOException ignored) {
            }
            return existingSetTimeout.asCallable().call(t, a);
          }));
  }

  static V6EventEmitterObject wrapAcceptedSocket(Socket sock) {
    V6EventEmitterObject s = new V6EventEmitterObject();
    s.setProto(V6EventEmitterConstructor.PROTOTYPE);
    wireSocket(sock, s);
    return s;
  }

  private static void wireUnixChannel(SocketChannel channel,
                                      V6EventEmitterObject s, String path) {
    wireGeneric(Channels.newInputStream(channel),
                Channels.newOutputStream(channel), channel, () -> {
                  try {
                    channel.shutdownOutput();
                  } catch (IOException ignored) {
                  }
                }, s, path, 0);
  }

  private static V6EventEmitterObject wrapUnixChannel(SocketChannel channel,
                                                      String path) {
    V6EventEmitterObject s = new V6EventEmitterObject();
    s.setProto(V6EventEmitterConstructor.PROTOTYPE);
    wireUnixChannel(channel, s, path);
    return s;
  }

  private static V6Value connectImpl(V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    String unixPath = null;
    int port = 0;
    String host = "localhost";
    if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof V6Object) {
      V6Object opts = (V6Object)first.ref();
      V6Value pathVal = opts.get("path");
      if (!pathVal.isUndefined()) {
        unixPath = pathVal.toString();
      } else {
        port = (int)opts.get("port").toNumber();
        V6Value hostVal = opts.get("host");
        if (!hostVal.isUndefined())
          host = hostVal.toString();
      }
    } else if (first.tag() == V6Value.TAG_STR) {
      unixPath = first.toString();
    } else {
      port = (int)first.toNumber();
      if (args.length > 1 && args[1].tag() == V6Value.TAG_STR)
        host = args[1].toString();
    }

    V6Callable connectCb = null;
    for (V6Value a : args)
      if (a.tag() == V6Value.TAG_FUNC)
        connectCb = a.asCallable();
    final V6Callable fConnectCb = connectCb;
    final String fUnixPath = unixPath;
    final int fPort = port;
    final String fHost = host;

    V6EventEmitterObject s = new V6EventEmitterObject();
    s.setProto(V6EventEmitterConstructor.PROTOTYPE);
    if (fConnectCb != null)
      s.get("on").asCallable().call(
          objValue(s), new V6Value[] {str("connect"), fn(fConnectCb)});
    s.set("write", fn((t, a) -> {
            throw new V6Throw(str("net: socket not yet connected"));
          }));
    s.set("destroy", fn((t, a) -> UNDEF));

    V6EventLoop.ref();
    Thread th = new Thread(() -> {
      try {
        if (fUnixPath != null) {
          SocketChannel channel =
              SocketChannel.open(UnixDomainSocketAddress.of(fUnixPath));
          wireUnixChannel(channel, s, fUnixPath);
        } else {
          Socket sock = new Socket(fHost, fPort);
          wireSocket(sock, s);
        }
        V6EventLoop.postExternal(
            ()
                -> s.get("emit").asCallable().call(
                    objValue(s), new V6Value[] {str("connect")}));
      } catch (IOException e) {
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
                  objValue(server),
                  new V6Value[] {str("connection"), fn(fConnListener)});

            ServerSocket[] serverSocketHolder = new ServerSocket[1];
            ServerSocketChannel[] unixServerHolder = new ServerSocketChannel[1];
            boolean[] closed = {false};

            server.set(
                "listen", fn((t, a) -> {
                  V6Value first = V6Value.argAt(a, 0);
                  String unixPath = null;
                  int port = 0;
                  if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof
                                                            V6Object) {
                    V6Object opts = (V6Object)first.ref();
                    V6Value pathVal = opts.get("path");
                    if (!pathVal.isUndefined())
                      unixPath = pathVal.toString();
                    else
                      port = (int)opts.get("port").toNumber();
                  } else if (first.tag() == V6Value.TAG_STR) {
                    unixPath = first.toString();
                  } else {
                    port = (int)first.toNumber();
                  }
                  V6Callable listenCb = null;
                  for (V6Value v : a)
                    if (v.tag() == V6Value.TAG_FUNC)
                      listenCb = v.asCallable();
                  final V6Callable fListenCb = listenCb;
                  final String fUnixPath = unixPath;
                  final int fPort = port;
                  try {
                    if (fUnixPath != null) {
                      File f = new File(fUnixPath);
                      if (f.exists())
                        f.delete();
                      ServerSocketChannel ssc =
                          ServerSocketChannel.open(StandardProtocolFamily.UNIX);
                      ssc.bind(UnixDomainSocketAddress.of(fUnixPath));
                      unixServerHolder[0] = ssc;
                      V6EventLoop.ref();
                      Thread th = new Thread(() -> {
                        try {
                          while (!closed[0]) {
                            SocketChannel channel = ssc.accept();
                            V6EventLoop.postExternal(() -> {
                              V6EventEmitterObject wrapped =
                                  wrapUnixChannel(channel, fUnixPath);
                              server.get("emit").asCallable().call(
                                  objValue(server),
                                  new V6Value[] {str("connection"),
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
                    } else {
                      ServerSocket ss = new ServerSocket(fPort);
                      serverSocketHolder[0] = ss;
                      V6EventLoop.ref();
                      Thread th = new Thread(() -> {
                        try {
                          while (!closed[0]) {
                            Socket sock = ss.accept();
                            V6EventLoop.postExternal(() -> {
                              V6EventEmitterObject wrapped =
                                  wrapAcceptedSocket(sock);
                              server.get("emit").asCallable().call(
                                  objValue(server),
                                  new V6Value[] {str("connection"),
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
                    }
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
                           if (unixServerHolder[0] != null)
                             unixServerHolder[0].close();
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

            server.set(
                "address", fn((t, a) -> {
                  if (serverSocketHolder[0] != null) {
                    V6Object addr = new V6Object();
                    addr.set("port",
                             new V6Value(V6Value.TAG_NUM,
                                         serverSocketHolder[0].getLocalPort(),
                                         null));
                    addr.set("address", str(serverSocketHolder[0]
                                                .getInetAddress()
                                                .getHostAddress()));
                    return objValue(addr);
                  }
                  if (unixServerHolder[0] != null) {
                    try {
                      return str(
                          unixServerHolder[0].getLocalAddress().toString());
                    } catch (IOException e) {
                      return NUL;
                    }
                  }
                  return NUL;
                }));

            return objValue(server);
          }));

    o.set("connect", fn((thisArg, args) -> connectImpl(args)));
    o.set("createConnection", fn((thisArg, args) -> connectImpl(args)));

    return o;
  }
}

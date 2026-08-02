import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;

public final class V6Dgram {
  private V6Dgram() {
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
  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);

  private static byte[] bytesOf(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ && v.ref() instanceof V6Buffer)
      return ((V6Buffer)v.ref()).toBytes();
    return v.toString().getBytes(StandardCharsets.UTF_8);
  }

  private static V6Callable extractCallback(V6Value[] args) {
    for (int i = args.length - 1; i >= 0; i--)
      if (args[i].tag() == V6Value.TAG_FUNC)
        return args[i].asCallable();
    return null;
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set(
        "createSocket", fn((thisArg, args) -> {
          V6EventEmitterObject sock = new V6EventEmitterObject();
          sock.setProto(V6EventEmitterConstructor.PROTOTYPE);
          DatagramSocket[] dsHolder = new DatagramSocket[1];
          boolean[] closed = {false};

          V6Callable createCb = extractCallback(args);
          if (createCb != null)
            sock.get("on").asCallable().call(
                objValue(sock), new V6Value[] {str("message"), fn(createCb)});

          sock.set("bind", fn((t, a) -> {
                     int port = a.length > 0 && a[0].tag() == V6Value.TAG_NUM
                                    ? (int)a[0].toNumber()
                                    : 0;
                     V6Callable cb = extractCallback(a);
                     try {
                       DatagramSocket ds = new DatagramSocket(port);
                       dsHolder[0] = ds;
                       V6EventLoop.ref();
                       Thread th = new Thread(() -> {
                         byte[] buf = new byte[65536];
                         try {
                           while (!closed[0]) {
                             DatagramPacket packet =
                                 new DatagramPacket(buf, buf.length);
                             ds.receive(packet);
                             byte[] data = java.util.Arrays.copyOf(
                                 packet.getData(), packet.getLength());
                             String addr = packet.getAddress().getHostAddress();
                             int rport = packet.getPort();
                             V6EventLoop.postExternal(() -> {
                               V6Object rinfo = new V6Object();
                               rinfo.set("address", str(addr));
                               rinfo.set("port", num(rport));
                               rinfo.set("family", str("IPv4"));
                               sock.get("emit").asCallable().call(
                                   objValue(sock),
                                   new V6Value[] {str("message"),
                                                  objValue(new V6Buffer(data)),
                                                  objValue(rinfo)});
                             });
                           }
                         } catch (IOException ignored) {
                         } finally {
                           V6EventLoop.unref();
                         }
                       });
                       th.setDaemon(true);
                       th.start();
                       if (cb != null)
                         V6MicrotaskQueue.enqueue(
                             () -> cb.call(UNDEF, new V6Value[0]));
                       V6MicrotaskQueue.enqueue(
                           ()
                               -> sock.get("emit").asCallable().call(
                                   objValue(sock),
                                   new V6Value[] {str("listening")}));
                     } catch (IOException e) {
                       V6MicrotaskQueue.enqueue(
                           ()
                               -> sock.get("emit").asCallable().call(
                                   objValue(sock),
                                   new V6Value[] {
                                       str("error"),
                                       str(String.valueOf(e.getMessage()))}));
                     }
                     return t;
                   }));

          sock.set(
              "send", fn((t, a) -> {
                byte[] data = bytesOf(V6Value.argAt(a, 0));
                int port = (int)V6Value.argAt(a, 1).toNumber();
                String address = a.length > 2 && a[2].tag() == V6Value.TAG_STR
                                     ? a[2].toString()
                                     : "localhost";
                V6Callable cb = extractCallback(a);
                try {
                  if (dsHolder[0] == null)
                    dsHolder[0] = new DatagramSocket();
                  DatagramSocket ds = dsHolder[0];
                  InetAddress addr = InetAddress.getByName(address);
                  DatagramPacket packet =
                      new DatagramPacket(data, data.length, addr, port);
                  ds.send(packet);
                  if (cb != null) {
                    V6Callable fcb = cb;
                    V6MicrotaskQueue.enqueue(
                        () -> fcb.call(UNDEF, new V6Value[] {NUL}));
                  }
                } catch (IOException e) {
                  if (cb != null) {
                    V6Callable fcb = cb;
                    V6MicrotaskQueue.enqueue(
                        ()
                            -> fcb.call(UNDEF,
                                        new V6Value[] {str(
                                            String.valueOf(e.getMessage()))}));
                  } else {
                    sock.get("emit").asCallable().call(
                        objValue(sock),
                        new V6Value[] {str("error"),
                                       str(String.valueOf(e.getMessage()))});
                  }
                }
                return UNDEF;
              }));

          sock.set("close", fn((t, a) -> {
                     closed[0] = true;
                     if (dsHolder[0] != null)
                       dsHolder[0].close();
                     V6Callable cb = extractCallback(a);
                     if (cb != null)
                       V6MicrotaskQueue.enqueue(
                           () -> cb.call(UNDEF, new V6Value[0]));
                     V6MicrotaskQueue.enqueue(
                         ()
                             -> sock.get("emit").asCallable().call(
                                 objValue(sock), new V6Value[] {str("close")}));
                     return UNDEF;
                   }));

          sock.set("address", fn((t, a) -> {
                     if (dsHolder[0] == null)
                       throw new V6Throw(str("dgram: socket not bound"));
                     V6Object addr = new V6Object();
                     addr.set(
                         "address",
                         str(dsHolder[0].getLocalAddress().getHostAddress()));
                     addr.set("port", num(dsHolder[0].getLocalPort()));
                     addr.set("family", str("IPv4"));
                     return objValue(addr);
                   }));

          sock.set("setBroadcast", fn((t, a) -> {
                     try {
                       if (dsHolder[0] != null)
                         dsHolder[0].setBroadcast(V6Value.argAt(a, 0).truthy());
                     } catch (IOException ignored) {
                     }
                     return UNDEF;
                   }));

          return objValue(sock);
        }));

    return o;
  }
}

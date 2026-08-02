import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.UnknownHostException;

public final class V6Dns {
  private V6Dns() {}

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

  private static V6Value resolveAll(V6Value[] args, boolean wantV6) {
    String host = V6Value.argAt(args, 0).toString();
    V6Callable cb = null;
    for (int i = 1; i < args.length; i++)
      if (args[i].tag() == V6Value.TAG_FUNC) {
        cb = args[i].asCallable();
        break;
      }
    final V6Callable fcb = cb;
    try {
      InetAddress[] all = InetAddress.getAllByName(host);
      V6Array result = new V6Array();
      for (InetAddress a : all)
        if ((a instanceof Inet6Address) == wantV6)
          result.push(str(a.getHostAddress()));
      final V6Array fresult = result;
      if (fcb != null)
        V6MicrotaskQueue.enqueue(
            () -> fcb.call(UNDEF, new V6Value[] {NUL, objValue(fresult)}));
    } catch (UnknownHostException e) {
      if (fcb != null)
        V6MicrotaskQueue.enqueue(
            () -> fcb.call(UNDEF, new V6Value[] {str("ENOTFOUND: " + host), UNDEF}));
    }
    return UNDEF;
  }

  private static V6Object buildPromises() {
    V6Object p = new V6Object();
    p.set("lookup", fn((thisArg, args) -> {
            V6Promise promise = new V6Promise();
            String host = V6Value.argAt(args, 0).toString();
            try {
              InetAddress addr = InetAddress.getByName(host);
              V6Object result = new V6Object();
              result.set("address", str(addr.getHostAddress()));
              result.set("family", new V6Value(V6Value.TAG_NUM,
                                               addr instanceof Inet6Address ? 6 : 4,
                                               null));
              promise.resolve(objValue(result));
            } catch (UnknownHostException e) {
              promise.reject(str("ENOTFOUND: " + host));
            }
            return objValue(promise);
          }));
    return p;
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("lookup", fn((thisArg, args) -> {
            String host = V6Value.argAt(args, 0).toString();
            V6Callable cb = null;
            for (int i = 1; i < args.length; i++)
              if (args[i].tag() == V6Value.TAG_FUNC) {
                cb = args[i].asCallable();
                break;
              }
            final V6Callable fcb = cb;
            try {
              InetAddress addr = InetAddress.getByName(host);
              String ip = addr.getHostAddress();
              int family = addr instanceof Inet6Address ? 6 : 4;
              if (fcb != null)
                V6MicrotaskQueue.enqueue(
                    ()
                        -> fcb.call(UNDEF,
                                   new V6Value[] {
                                       NUL, str(ip),
                                       new V6Value(V6Value.TAG_NUM, family, null)}));
            } catch (UnknownHostException e) {
              if (fcb != null)
                V6MicrotaskQueue.enqueue(
                    ()
                        -> fcb.call(UNDEF, new V6Value[] {
                             str("ENOTFOUND: " + host), UNDEF, UNDEF
                           }));
            }
            return UNDEF;
          }));

    o.set("resolve4", fn((thisArg, args) -> resolveAll(args, false)));
    o.set("resolve6", fn((thisArg, args) -> resolveAll(args, true)));

    o.set("promises", objValue(buildPromises()));
    return o;
  }
}

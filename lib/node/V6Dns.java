import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Hashtable;
import javax.naming.Context;
import javax.naming.NamingEnumeration;
import javax.naming.directory.Attribute;
import javax.naming.directory.Attributes;
import javax.naming.directory.DirContext;
import javax.naming.directory.InitialDirContext;

public final class V6Dns {
  private V6Dns() {
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

  private static V6Value resolveAll(V6Value[] args, boolean wantV6) {
    String host = V6Value.argAt(args, 0).toString();
    V6Callable cb = extractCallback(args);
    try {
      InetAddress[] all = InetAddress.getAllByName(host);
      V6Array result = new V6Array();
      for (InetAddress a : all)
        if ((a instanceof Inet6Address) == wantV6)
          result.push(str(a.getHostAddress()));
      final V6Array fresult = result;
      if (cb != null)
        V6MicrotaskQueue.enqueue(
            () -> cb.call(UNDEF, new V6Value[] {NUL, objValue(fresult)}));
    } catch (UnknownHostException e) {
      if (cb != null)
        V6MicrotaskQueue.enqueue(
            ()
                -> cb.call(UNDEF,
                           new V6Value[] {str("ENOTFOUND: " + host), UNDEF}));
    }
    return UNDEF;
  }

  private static V6Callable extractCallback(V6Value[] args) {
    for (int i = args.length - 1; i >= 0; i--)
      if (args[i].tag() == V6Value.TAG_FUNC)
        return args[i].asCallable();
    return null;
  }

  private static String stripTrailingDot(String s) {
    return s.endsWith(".") ? s.substring(0, s.length() - 1) : s;
  }

  private static DirContext dnsContext(String[] servers) throws Exception {
    Hashtable<String, String> env = new Hashtable<>();
    env.put(Context.INITIAL_CONTEXT_FACTORY,
            "com.sun.jndi.dns.DnsContextFactory");
    if (servers != null && servers.length > 0) {
      StringBuilder sb = new StringBuilder();
      for (String s : servers) {
        if (sb.length() > 0)
          sb.append(' ');
        sb.append("dns://").append(s);
      }
      env.put(Context.PROVIDER_URL, sb.toString());
    }
    return new InitialDirContext(env);
  }

  private static V6Value recordValue(String type, String raw) {
    if (type.equals("MX")) {
      String[] parts = raw.trim().split("\\s+", 2);
      V6Object mx = new V6Object();
      mx.set("priority",
             new V6Value(V6Value.TAG_NUM,
                         parts.length > 0 ? Double.parseDouble(parts[0]) : 0,
                         null));
      mx.set("exchange",
             str(stripTrailingDot(parts.length > 1 ? parts[1] : "")));
      return objValue(mx);
    }
    if (type.equals("TXT")) {
      String cleaned = raw.replaceAll("^\"|\"$", "");
      V6Array inner = new V6Array();
      inner.push(str(cleaned));
      return objValue(inner);
    }
    return str(stripTrailingDot(raw));
  }

  private static void resolveType(String[] servers, String hostname,
                                  String type, V6Callable cb) {
    try {
      DirContext ctx = dnsContext(servers);
      Attributes attrs = ctx.getAttributes(hostname, new String[] {type});
      Attribute attr = attrs.get(type);
      V6Array result = new V6Array();
      if (attr != null) {
        NamingEnumeration<?> e = attr.getAll();
        while (e.hasMore())
          result.push(recordValue(type, e.next().toString()));
      }
      V6MicrotaskQueue.enqueue(
          () -> cb.call(UNDEF, new V6Value[] {NUL, objValue(result)}));
    } catch (Exception e) {
      V6MicrotaskQueue.enqueue(
          ()
              -> cb.call(UNDEF,
                         new V6Value[] {str("ENOTFOUND: " + hostname), UNDEF}));
    }
  }

  private static V6Value typedResolve(String[] servers, V6Value[] args,
                                      String type) {
    String host = V6Value.argAt(args, 0).toString();
    V6Callable cb = extractCallback(args);
    if (cb != null)
      resolveType(servers, host, type, cb);
    return UNDEF;
  }

  static V6Value resolve4Impl(String[] servers, V6Value[] args) {
    return typedResolve(servers, args, "A");
  }

  static V6Value resolve6Impl(String[] servers, V6Value[] args) {
    return typedResolve(servers, args, "AAAA");
  }

  static V6Value resolveMxImpl(String[] servers, V6Value[] args) {
    return typedResolve(servers, args, "MX");
  }

  static V6Value resolveTxtImpl(String[] servers, V6Value[] args) {
    return typedResolve(servers, args, "TXT");
  }

  static V6Value resolveCnameImpl(String[] servers, V6Value[] args) {
    return typedResolve(servers, args, "CNAME");
  }

  static V6Value resolveNsImpl(String[] servers, V6Value[] args) {
    return typedResolve(servers, args, "NS");
  }

  private static String reverseArpaName(String ip) {
    String[] parts = ip.split("\\.");
    StringBuilder sb = new StringBuilder();
    for (int i = parts.length - 1; i >= 0; i--)
      sb.append(parts[i]).append('.');
    sb.append("in-addr.arpa");
    return sb.toString();
  }

  static V6Value reverseImpl(String[] servers, V6Value[] args) {
    String ip = V6Value.argAt(args, 0).toString();
    V6Callable cb = extractCallback(args);
    if (cb != null)
      resolveType(servers, reverseArpaName(ip), "PTR", cb);
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
              result.set("family",
                         new V6Value(V6Value.TAG_NUM,
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
            V6Callable cb = extractCallback(args);
            try {
              InetAddress addr = InetAddress.getByName(host);
              String ip = addr.getHostAddress();
              int family = addr instanceof Inet6Address ? 6 : 4;
              if (cb != null)
                V6MicrotaskQueue.enqueue(
                    ()
                        -> cb.call(UNDEF,
                                   new V6Value[] {NUL, str(ip),
                                                  new V6Value(V6Value.TAG_NUM,
                                                              family, null)}));
            } catch (UnknownHostException e) {
              if (cb != null)
                V6MicrotaskQueue.enqueue(
                    ()
                        -> cb.call(UNDEF,
                                   new V6Value[] {str("ENOTFOUND: " + host),
                                                  UNDEF, UNDEF}));
            }
            return UNDEF;
          }));

    o.set("resolve4", fn((thisArg, args) -> resolveAll(args, false)));
    o.set("resolve6", fn((thisArg, args) -> resolveAll(args, true)));
    o.set("resolveMx", fn((thisArg, args) -> resolveMxImpl(null, args)));
    o.set("resolveTxt", fn((thisArg, args) -> resolveTxtImpl(null, args)));
    o.set("resolveCname", fn((thisArg, args) -> resolveCnameImpl(null, args)));
    o.set("resolveNs", fn((thisArg, args) -> resolveNsImpl(null, args)));
    o.set("reverse", fn((thisArg, args) -> reverseImpl(null, args)));
    o.set("Resolver", objValue(new V6DnsResolverConstructor()));

    o.set("promises", objValue(buildPromises()));
    return o;
  }

  public static final V6Value MODULE = objValue(build());
}

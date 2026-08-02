public final class V6DnsResolverConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6DnsResolverConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
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

  private static String[] serversOf(V6Value thisArg) {
    if (!(thisArg.ref() instanceof V6Object))
      return null;
    V6Object self = (V6Object)thisArg.ref();
    V6Value v = self.get("_servers");
    if (v.tag() != V6Value.TAG_OBJ || !(v.ref() instanceof V6Array))
      return null;
    V6Array arr = (V6Array)v.ref();
    if (arr.elemCount == 0)
      return null;
    String[] out = new String[arr.elemCount];
    for (int i = 0; i < arr.elemCount; i++)
      out[i] = arr.elements[i].toString();
    return out;
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.set("setServers", fn((t, a) -> {
            ((V6Object)t.ref()).set("_servers", V6Value.argAt(a, 0));
            return UNDEF;
          }));

    o.set("getServers", fn((t, a) -> {
            String[] servers = serversOf(t);
            V6Array result = new V6Array();
            if (servers != null)
              for (String s : servers)
                result.push(str(s));
            return objValue(result);
          }));

    o.set("resolve4", fn((t, a) -> V6Dns.resolve4Impl(serversOf(t), a)));
    o.set("resolve6", fn((t, a) -> V6Dns.resolve6Impl(serversOf(t), a)));
    o.set("resolveMx", fn((t, a) -> V6Dns.resolveMxImpl(serversOf(t), a)));
    o.set("resolveTxt", fn((t, a) -> V6Dns.resolveTxtImpl(serversOf(t), a)));
    o.set("resolveCname",
          fn((t, a) -> V6Dns.resolveCnameImpl(serversOf(t), a)));
    o.set("resolveNs", fn((t, a) -> V6Dns.resolveNsImpl(serversOf(t), a)));
    o.set("reverse", fn((t, a) -> V6Dns.reverseImpl(serversOf(t), a)));
    o.set("cancel", fn((t, a) -> UNDEF));

    return o;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object self = new V6Object();
    self.setProto(PROTOTYPE);
    return objValue(self);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }
}

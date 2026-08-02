public final class V6HttpAgentConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6HttpAgentConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.set("destroy", fn((t, a) -> new V6Value(V6Value.TAG_UNDEF, 0, null)));
    return o;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object agent = new V6Object();
    agent.setProto(PROTOTYPE);
    V6Value keepAlive = new V6Value(V6Value.TAG_BOOL, 0, null);
    if (args.length > 0 && args[0].tag() == V6Value.TAG_OBJ)
      keepAlive = ((V6Object)args[0].ref()).get("keepAlive");
    agent.set("keepAlive", keepAlive);
    return new V6Value(V6Value.TAG_OBJ, 0, agent);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }
}

public final class V6MessageChannelConstructor extends V6Object
    implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = new V6Object();

  public V6MessageChannelConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object channel = new V6Object();
    V6Object port1 = V6WorkerThreads.buildPort();
    V6Object port2 = V6WorkerThreads.buildPort();
    port1.get("_setPeer").asCallable().call(objValue(port1), new V6Value[] {objValue(port2)});
    port2.get("_setPeer").asCallable().call(objValue(port2), new V6Value[] {objValue(port1)});
    port1.props.remove("_setPeer");
    port2.props.remove("_setPeer");
    channel.set("port1", objValue(port1));
    channel.set("port2", objValue(port2));
    return objValue(channel);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }
}

public final class V6WebMessageChannelConstructor
    extends V6Object implements V6NativeConstructor {
  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6MessagePortObject port1 = V6MessagePortConstructor.newPort();
    V6MessagePortObject port2 = V6MessagePortConstructor.newPort();
    port1.peer = port2;
    port2.peer = port1;
    V6Object channel = new V6Object();
    channel.set("port1", objValue(port1));
    channel.set("port2", objValue(port2));
    return objValue(channel);
  }
}

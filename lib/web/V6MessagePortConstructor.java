public final class V6MessagePortConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6MessagePortConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
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

  @Override
  public V6Value construct(V6Value[] args) {
    throw new V6Throw(str("TypeError: Illegal constructor"));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  static V6MessagePortObject newPort() {
    V6MessagePortObject p = new V6MessagePortObject();
    p.setProto(PROTOTYPE);
    return p;
  }

  private static V6MessagePortObject self(V6Value t) {
    return (V6MessagePortObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventTargetConstructor.PROTOTYPE);

    o.set("postMessage", fn((t, a) -> {
            V6MessagePortObject port = self(t);
            V6MessagePortObject peer = port.peer;
            if (peer != null) {
              V6Value cloned = V6StructuredClone.clone(V6Value.argAt(a, 0));
              V6MicrotaskQueue.enqueue(() -> {
                V6MessageEventObject ev = new V6MessageEventObject();
                ev.setProto(V6MessageEventConstructor.PROTOTYPE);
                ev.type = "message";
                ev.data = cloned;
                peer.dispatch(ev);
              });
            }
            return UNDEF;
          }));
    o.set("start", fn((t, a) -> UNDEF));
    o.set("close", fn((t, a) -> {
            V6EventObject ev = new V6EventObject();
            ev.setProto(V6EventConstructor.PROTOTYPE);
            ev.type = "close";
            self(t).dispatch(ev);
            return UNDEF;
          }));

    V6EventHandlerProperty.install(o, "onmessage", "message");
    V6EventHandlerProperty.install(o, "onmessageerror", "messageerror");

    return o;
  }
}

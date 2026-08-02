public final class V6WorkerThreads {
  private V6WorkerThreads() {}

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

  static V6Object buildPort() {
    V6EventEmitterObject port = new V6EventEmitterObject();
    port.setProto(V6EventEmitterConstructor.PROTOTYPE);
    V6EventEmitterObject[] peerHolder = new V6EventEmitterObject[1];

    port.set("postMessage", fn((t, a) -> {
            V6Value data = V6Value.argAt(a, 0);
            V6EventEmitterObject peer = peerHolder[0];
            if (peer != null)
              V6MicrotaskQueue.enqueue(
                  ()
                      -> peer.get("emit").asCallable().call(
                          objValue(peer), new V6Value[] {str("message"), data}));
            return UNDEF;
          }));
    port.set("close", fn((t, a) -> {
            port.get("emit").asCallable().call(objValue(port), new V6Value[] {str("close")});
            return UNDEF;
          }));
    port.set("start", fn((t, a) -> UNDEF));
    port.set("unref", fn((t, a) -> UNDEF));
    port.set("ref", fn((t, a) -> UNDEF));

    port.set("_setPeer", fn((t, a) -> {
            peerHolder[0] = (V6EventEmitterObject)V6Value.argAt(a, 0).ref();
            return UNDEF;
          }));

    return port;
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("MessageChannel",
          new V6Value(V6Value.TAG_OBJ, 0, new V6MessageChannelConstructor()));

    o.set("Worker", new V6Value(V6Value.TAG_OBJ, 0, new V6WorkerConstructor()));

    o.set("isMainThread", new V6Value(V6Value.TAG_BOOL, 1, null));
    o.set("threadId", new V6Value(V6Value.TAG_NUM, 0, null));
    o.set("parentPort", NUL);
    o.set("workerData", UNDEF);

    return o;
  }
}

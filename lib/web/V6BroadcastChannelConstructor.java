import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

public final class V6BroadcastChannelConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  private static final Map<String, List<V6BroadcastChannelObject>> REGISTRY =
      new ConcurrentHashMap<>();

  public V6BroadcastChannelConstructor() {
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
  public V6Object allocate() {
    return new V6BroadcastChannelObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6BroadcastChannelObject ch = new V6BroadcastChannelObject();
    ch.setProto(PROTOTYPE);
    ch.name = V6Value.argAt(args, 0).toString();
    REGISTRY.computeIfAbsent(ch.name, k -> new CopyOnWriteArrayList<>())
        .add(ch);
    return objValue(ch);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6BroadcastChannelObject self(V6Value t) {
    return (V6BroadcastChannelObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventTargetConstructor.PROTOTYPE);

    o.defineGetter("name", (t, a) -> str(self(t).name));

    o.set("postMessage", fn((t, a) -> {
            V6BroadcastChannelObject ch = self(t);
            if (ch.closed)
              throw new V6Throw(str("InvalidStateError: channel is closed"));
            V6Value data = V6Value.argAt(a, 0);
            List<V6BroadcastChannelObject> peers = REGISTRY.get(ch.name);
            if (peers != null) {
              for (V6BroadcastChannelObject peer : peers) {
                if (peer == ch || peer.closed)
                  continue;
                V6Value cloned = V6StructuredClone.clone(data);
                V6MicrotaskQueue.enqueue(() -> {
                  if (peer.closed)
                    return;
                  V6MessageEventObject ev = new V6MessageEventObject();
                  ev.setProto(V6MessageEventConstructor.PROTOTYPE);
                  ev.type = "message";
                  ev.data = cloned;
                  peer.dispatch(ev);
                });
              }
            }
            return UNDEF;
          }));

    o.set("close", fn((t, a) -> {
            V6BroadcastChannelObject ch = self(t);
            ch.closed = true;
            List<V6BroadcastChannelObject> peers = REGISTRY.get(ch.name);
            if (peers != null)
              peers.remove(ch);
            return UNDEF;
          }));

    V6EventHandlerProperty.install(o, "onmessage", "message");
    V6EventHandlerProperty.install(o, "onmessageerror", "messageerror");

    return o;
  }
}

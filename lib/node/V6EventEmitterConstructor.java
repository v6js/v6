import java.util.ArrayList;
import java.util.List;

public final class V6EventEmitterConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6EventEmitterConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  @Override
  public V6Object allocate() {
    return new V6EventEmitterObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6EventEmitterObject e = new V6EventEmitterObject();
    e.setProto(PROTOTYPE);
    initInstance(e, args);
    return new V6Value(V6Value.TAG_OBJ, 0, e);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final java.util
      .WeakHashMap<Object, V6EventEmitterObject> MIXIN_STATE =
      new java.util.WeakHashMap<>();

  private static V6EventEmitterObject self(V6Value thisArg) {
    Object key = thisArg.ref();
    Object resolved =
        key instanceof V6Object ? ((V6Object)key).resolveEmitterTarget() : key;
    if (resolved instanceof V6EventEmitterObject)
      return (V6EventEmitterObject)resolved;
    return MIXIN_STATE.computeIfAbsent(resolved,
                                       k -> new V6EventEmitterObject());
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.set("on", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            String event = V6Value.argAt(args, 0).toString();
            V6Value listener = V6Value.argAt(args, 1);
            e.listenersFor(event, true).add(listener);
            return thisArg;
          }));
    o.set("addListener", o.get("on"));

    o.set("prependListener", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            String event = V6Value.argAt(args, 0).toString();
            V6Value listener = V6Value.argAt(args, 1);
            e.listenersFor(event, true).add(0, listener);
            return thisArg;
          }));

    o.set("prependOnceListener", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            String event = V6Value.argAt(args, 0).toString();
            V6Value listener = V6Value.argAt(args, 1);
            V6Value[] wrapperHolder = new V6Value[1];
            wrapperHolder[0] = fn((t, a) -> {
              List<V6Value> list = e.listenersFor(event, false);
              if (list != null)
                list.remove(wrapperHolder[0]);
              return listener.asCallable().call(t, a);
            });
            e.listenersFor(event, true).add(0, wrapperHolder[0]);
            return thisArg;
          }));

    o.set("once", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            String event = V6Value.argAt(args, 0).toString();
            V6Value listener = V6Value.argAt(args, 1);
            V6Value[] wrapperHolder = new V6Value[1];
            wrapperHolder[0] = fn((t, a) -> {
              List<V6Value> list = e.listenersFor(event, false);
              if (list != null)
                list.remove(wrapperHolder[0]);
              return listener.asCallable().call(t, a);
            });
            e.listenersFor(event, true).add(wrapperHolder[0]);
            return thisArg;
          }));

    o.set("off", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            String event = V6Value.argAt(args, 0).toString();
            V6Value listener = V6Value.argAt(args, 1);
            List<V6Value> list = e.listenersFor(event, false);
            if (list != null)
              list.remove(listener);
            return thisArg;
          }));
    o.set("removeListener", o.get("off"));

    o.set("emit", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            String event = V6Value.argAt(args, 0).toString();
            List<V6Value> list = e.listenersFor(event, false);
            if (list == null || list.isEmpty()) {
              if (event.equals("error"))
                throw new V6Throw(
                    args.length > 1
                        ? args[1]
                        : new V6Value(V6Value.TAG_STR, 0, "Unhandled error."));
              return new V6Value(V6Value.TAG_BOOL, 0, null);
            }
            V6Value[] rest = new V6Value[args.length - 1];
            System.arraycopy(args, 1, rest, 0, rest.length);
            List<V6Value> snapshot = new ArrayList<>(list);
            for (V6Value l : snapshot)
              l.asCallable().call(thisArg, rest);
            return new V6Value(V6Value.TAG_BOOL, 1, null);
          }));

    o.set("removeAllListeners", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            if (args.length == 0)
              e.listeners.clear();
            else
              e.listeners.remove(V6Value.argAt(args, 0).toString());
            return thisArg;
          }));

    o.set("listenerCount", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            List<V6Value> list =
                e.listenersFor(V6Value.argAt(args, 0).toString(), false);
            return new V6Value(V6Value.TAG_NUM, list == null ? 0 : list.size(),
                               null);
          }));

    o.set("listeners", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            List<V6Value> list =
                e.listenersFor(V6Value.argAt(args, 0).toString(), false);
            V6Array result = new V6Array();
            if (list != null)
              for (V6Value l : list)
                result.push(l);
            return objValue(result);
          }));

    o.set("eventNames", fn((thisArg, args) -> {
            V6EventEmitterObject e = self(thisArg);
            V6Array result = new V6Array();
            for (String k : e.listeners.keySet())
              if (!e.listeners.get(k).isEmpty())
                result.push(new V6Value(V6Value.TAG_STR, 0, k));
            return objValue(result);
          }));

    o.set("setMaxListeners", fn((thisArg, args) -> thisArg));

    return o;
  }
}

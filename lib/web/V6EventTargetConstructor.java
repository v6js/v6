public final class V6EventTargetConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6EventTargetConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  @Override
  public V6Object allocate() {
    return new V6EventTargetObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6EventTargetObject e = new V6EventTargetObject();
    e.setProto(PROTOTYPE);
    return new V6Value(V6Value.TAG_OBJ, 0, e);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6EventTargetObject self(V6Value t) {
    return (V6EventTargetObject)t.ref();
  }

  static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.set("addEventListener", fn((t, a) -> {
            String type = V6Value.argAt(a, 0).toString();
            V6Value listener = V6Value.argAt(a, 1);
            V6Value optsVal = V6Value.argAt(a, 2);
            boolean once = false;
            V6Value signal = null;
            if (optsVal.tag() == V6Value.TAG_OBJ && optsVal.ref() instanceof
                                                        V6Object) {
              V6Object opts = (V6Object)optsVal.ref();
              once = opts.get("once").truthy();
              V6Value s = opts.get("signal");
              if (!s.isUndefined())
                signal = s;
            }
            self(t).addListener(type, listener, once, signal);
            return UNDEF;
          }));

    o.set("removeEventListener", fn((t, a) -> {
            String type = V6Value.argAt(a, 0).toString();
            V6Value listener = V6Value.argAt(a, 1);
            self(t).removeListener(type, listener);
            return UNDEF;
          }));

    o.set("dispatchEvent", fn((t, a) -> {
            V6Value eventVal = V6Value.argAt(a, 0);
            if (eventVal.tag() != V6Value.TAG_OBJ ||
                !(eventVal.ref() instanceof V6EventObject))
              throw new V6Throw(
                  new V6Value(V6Value.TAG_STR, 0,
                              "dispatchEvent requires an Event argument"));
            boolean result = self(t).dispatch((V6EventObject)eventVal.ref());
            return new V6Value(V6Value.TAG_BOOL, result ? 1 : 0, null);
          }));

    return o;
  }
}

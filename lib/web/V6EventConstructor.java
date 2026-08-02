public class V6EventConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6EventConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  static void applyInit(V6EventObject e, V6Value initVal) {
    if (initVal.tag() == V6Value.TAG_OBJ && initVal.ref() instanceof V6Object) {
      V6Object init = (V6Object)initVal.ref();
      V6Value b = init.get("bubbles");
      if (!b.isUndefined())
        e.bubbles = b.truthy();
      V6Value c = init.get("cancelable");
      if (!c.isUndefined())
        e.cancelable = c.truthy();
      V6Value co = init.get("composed");
      if (!co.isUndefined())
        e.composed = co.truthy();
    }
  }

  @Override
  public V6Object allocate() {
    return new V6EventObject();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6EventObject e = new V6EventObject();
    e.setProto(PROTOTYPE);
    initInstance(e, args);
    return objValue(e);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6EventObject e = (V6EventObject)instance;
    e.type = V6Value.argAt(args, 0).toString();
    applyInit(e, V6Value.argAt(args, 1));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  static V6EventObject self(V6Value t) {
    return (V6EventObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.defineGetter("type", (t, a) -> str(self(t).type));
    o.defineGetter("target", (t, a) -> self(t).target);
    o.defineGetter("currentTarget", (t, a) -> self(t).currentTarget);
    o.defineGetter("bubbles", (t, a) -> bool(self(t).bubbles));
    o.defineGetter("cancelable", (t, a) -> bool(self(t).cancelable));
    o.defineGetter("composed", (t, a) -> bool(self(t).composed));
    o.defineGetter("defaultPrevented",
                   (t, a) -> bool(self(t).defaultPrevented));
    o.defineGetter("timeStamp", (t, a) -> num(self(t).timeStamp));
    o.defineGetter("isTrusted", (t, a) -> bool(false));
    o.defineGetter("eventPhase", (t, a) -> num(0));

    o.set("preventDefault", fn((t, a) -> {
            V6EventObject e = self(t);
            if (e.cancelable)
              e.defaultPrevented = true;
            return UNDEF;
          }));
    o.set("stopPropagation", fn((t, a) -> {
            self(t).propagationStopped = true;
            return UNDEF;
          }));
    o.set("stopImmediatePropagation", fn((t, a) -> {
            V6EventObject e = self(t);
            e.propagationStopped = true;
            e.immediatePropagationStopped = true;
            return UNDEF;
          }));
    o.set("composedPath", fn((t, a) -> {
            V6Array result = new V6Array();
            V6Value target = self(t).target;
            if (target.tag() != V6Value.TAG_NULL &&
                target.tag() != V6Value.TAG_UNDEF)
              result.push(target);
            return objValue(result);
          }));

    return o;
  }
}

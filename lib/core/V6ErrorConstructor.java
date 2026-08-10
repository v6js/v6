public final class V6ErrorConstructor
    extends V6Object implements V6NativeConstructor {
  public final V6Object protoObj;
  public final String fixedName;
  public static double stackTraceLimit = 10;
  public static V6Value prepareStackTrace =
      new V6Value(V6Value.TAG_UNDEF, 0, null);

  public V6ErrorConstructor(V6Object protoObj, String fixedName) {
    this.protoObj = protoObj;
    this.fixedName = fixedName;
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, protoObj));
    protoObj.nativeCtor = this;
    set("captureStackTrace",
        new V6Value(V6Value.TAG_FUNC, 0, (V6Callable)(thisArg, args) -> {
          V6Value target = V6Value.argAt(args, 0);
          if (target.tag() == V6Value.TAG_OBJ && target.ref() instanceof
                                                     V6Object)
            captureStack((V6Object)target.ref());
          return new V6Value(V6Value.TAG_UNDEF, 0, null);
        }));
    o_defineAccessors();
  }

  private void o_defineAccessors() {
    defineGetter("stackTraceLimit",
                 (t, a) -> new V6Value(V6Value.TAG_NUM, stackTraceLimit, null));
    defineSetter("stackTraceLimit", (t, a) -> {
      stackTraceLimit = V6Value.argAt(a, 0).toNumber();
      return new V6Value(V6Value.TAG_UNDEF, 0, null);
    });
    defineGetter("prepareStackTrace", (t, a) -> prepareStackTrace);
    defineSetter("prepareStackTrace", (t, a) -> {
      prepareStackTrace = V6Value.argAt(a, 0);
      return new V6Value(V6Value.TAG_UNDEF, 0, null);
    });
  }

  static String stackHead(V6Object instance) {
    V6Value nameVal = instance.get("name");
    String name =
        nameVal.tag() == V6Value.TAG_STR ? nameVal.toString() : "Error";
    V6Value msgVal = instance.get("message");
    String msg = msgVal.tag() == V6Value.TAG_STR ? msgVal.toString() : "";
    return msg.isEmpty() ? name : name + ": " + msg;
  }

  static void captureStack(V6Object instance) {
    V6Value instanceVal = new V6Value(V6Value.TAG_OBJ, 0, instance);
    if (prepareStackTrace.tag() == V6Value.TAG_FUNC) {
      V6Value callSites = new V6Value(V6Value.TAG_OBJ, 0,
                                      V6CaptureCallSites.captureCallSites(2));
      V6Value result = prepareStackTrace.call(
          instanceVal, new V6Value[] {instanceVal, callSites});
      instance.set("stack", result);
      return;
    }
    instance.set("stack", new V6Value(V6Value.TAG_STR, 0, stackHead(instance)));
  }

  @Override
  public V6Object allocate() {
    return new V6Object();
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Object instance = new V6Object();
    instance.setProto(protoObj);
    initInstance(instance, args);
    return new V6Value(V6Value.TAG_OBJ, 0, instance);
  }

  @Override
  public void initInstance(V6Object instance, V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    String msg = first.isUndefined() ? "" : first.toString();
    instance.set("message", new V6Value(V6Value.TAG_STR, 0, msg));
    String name;
    if (fixedName != null) {
      name = fixedName;
    } else {
      V6Value second = V6Value.argAt(args, 1);
      name = second.isUndefined() ? "Error" : second.toString();
    }
    instance.set("name", new V6Value(V6Value.TAG_STR, 0, name));
    captureStack(instance);
  }

  @Override
  public V6Object prototypeObject() {
    return protoObj;
  }
}

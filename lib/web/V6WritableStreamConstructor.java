public final class V6WritableStreamConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6WritableStreamConstructor() {
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

  private static V6Value num(double n) {
    return new V6Value(V6Value.TAG_NUM, n, null);
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public static V6WritableStreamObject newStream(V6Value underlyingSink) {
    V6WritableStreamObject s = new V6WritableStreamObject();
    s.setProto(PROTOTYPE);

    V6Object controller = new V6Object();
    controller.set("error", fn((t, a) -> {
                     s.errored = true;
                     s.errorReason = V6Value.argAt(a, 0);
                     return UNDEF;
                   }));
    s.controller = controller;

    if (underlyingSink.tag() == V6Value.TAG_OBJ &&
        underlyingSink.ref() instanceof V6Object) {
      V6Object sink = (V6Object)underlyingSink.ref();
      V6Value writeVal = sink.get("write");
      if (writeVal.tag() == V6Value.TAG_FUNC)
        s.writeFn = writeVal.asCallable();
      V6Value closeVal = sink.get("close");
      if (closeVal.tag() == V6Value.TAG_FUNC)
        s.closeFn = closeVal.asCallable();
      V6Value abortVal = sink.get("abort");
      if (abortVal.tag() == V6Value.TAG_FUNC)
        s.abortFn = abortVal.asCallable();
      V6Value startVal = sink.get("start");
      if (startVal.tag() == V6Value.TAG_FUNC)
        startVal.asCallable().call(underlyingSink,
                                   new V6Value[] {objValue(controller)});
    }
    return s;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    return objValue(newStream(V6Value.argAt(args, 0)));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6WritableStreamObject self(V6Value t) {
    return (V6WritableStreamObject)t.ref();
  }

  private static V6Object buildWriter(V6WritableStreamObject s) {
    V6Object writer = new V6Object();
    writer.set("write", fn((t, a) -> objValue(s.write(V6Value.argAt(a, 0)))));
    writer.set("close", fn((t, a) -> objValue(s.close())));
    writer.set("abort", fn((t, a) -> objValue(s.abort(V6Value.argAt(a, 0)))));
    writer.set("releaseLock", fn((t, a) -> {
                 s.locked = false;
                 return UNDEF;
               }));
    writer.set("closed", objValue(s.closedPromise));
    writer.set("ready", objValue(V6Promise.resolved(UNDEF)));
    writer.defineGetter("desiredSize",
                        (t, a) -> num(Math.max(0, 1 - s.chunkQueue.size())));
    return writer;
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.defineGetter("locked", (t, a) -> bool(self(t).locked));

    o.set("getWriter", fn((t, a) -> {
            V6WritableStreamObject s = self(t);
            if (s.locked)
              throw new V6Throw(
                  str("TypeError: WritableStream is already locked"));
            s.locked = true;
            return objValue(buildWriter(s));
          }));

    o.set("close", fn((t, a) -> objValue(self(t).close())));
    o.set("abort", fn((t, a) -> objValue(self(t).abort(V6Value.argAt(a, 0)))));
    o.set("write", fn((t, a) -> objValue(self(t).write(V6Value.argAt(a, 0)))));

    return o;
  }
}

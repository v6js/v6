public final class V6ReadableStreamConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6ReadableStreamConstructor() {
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

  public static V6ReadableStreamObject newStream(V6Value underlyingSource) {
    V6ReadableStreamObject s = new V6ReadableStreamObject();
    s.setProto(PROTOTYPE);

    V6Object controller = new V6Object();
    controller.set("enqueue", fn((t, a) -> {
                     s.enqueue(V6Value.argAt(a, 0));
                     return UNDEF;
                   }));
    controller.set("close", fn((t, a) -> {
                     s.closeStream();
                     return UNDEF;
                   }));
    controller.set("error", fn((t, a) -> {
                     s.errorStream(V6Value.argAt(a, 0));
                     return UNDEF;
                   }));
    controller.defineGetter("desiredSize",
                            (t, a) -> num(Math.max(0, 1 - s.queue.size())));
    s.controller = controller;

    if (underlyingSource.tag() == V6Value.TAG_OBJ &&
        underlyingSource.ref() instanceof V6Object) {
      V6Object src = (V6Object)underlyingSource.ref();
      V6Value pullVal = src.get("pull");
      if (pullVal.tag() == V6Value.TAG_FUNC)
        s.pullFn = pullVal.asCallable();
      V6Value cancelVal = src.get("cancel");
      if (cancelVal.tag() == V6Value.TAG_FUNC)
        s.cancelFn = cancelVal.asCallable();
      V6Value startVal = src.get("start");
      if (startVal.tag() == V6Value.TAG_FUNC)
        startVal.asCallable().call(underlyingSource,
                                   new V6Value[] {objValue(controller)});
    }
    return s;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    V6Value underlyingSource = V6Value.argAt(args, 0);
    return objValue(newStream(underlyingSource));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6ReadableStreamObject self(V6Value t) {
    return (V6ReadableStreamObject)t.ref();
  }

  private static V6Object buildReader(V6ReadableStreamObject s) {
    V6Object reader = new V6Object();
    reader.set("read", fn((t, a) -> objValue(s.read())));
    reader.set("releaseLock", fn((t, a) -> {
                 s.locked = false;
                 return UNDEF;
               }));
    reader.set("cancel", fn((t, a) -> {
                 V6Value reason = V6Value.argAt(a, 0);
                 s.queue.clear();
                 s.closeStream();
                 if (s.cancelFn != null)
                   return objValue(V6Promise.resolved(
                       s.cancelFn.call(UNDEF, new V6Value[] {reason})));
                 return objValue(V6Promise.resolved(UNDEF));
               }));
    reader.set("closed", objValue(s.closedPromise));
    return reader;
  }

  static void pipeChunks(V6ReadableStreamObject source, V6Object writer,
                         V6Promise done, boolean preventClose,
                         boolean preventAbort) {
    V6Promise readPromise = source.read();
    readPromise.addCallbacks(
        (resultVal)
            -> {
          V6Object result = (V6Object)resultVal.ref();
          if (result.get("done").truthy()) {
            if (!preventClose)
              writer.get("close").asCallable().call(objValue(writer),
                                                    new V6Value[0]);
            done.resolve(UNDEF);
            return;
          }
          V6Value writePromiseVal = writer.get("write").asCallable().call(
              objValue(writer), new V6Value[] {result.get("value")});
          V6Promise writePromise = V6Promise.resolved(writePromiseVal);
          writePromise.addCallbacks((okVal)
                                        -> pipeChunks(source, writer, done,
                                                      preventClose,
                                                      preventAbort),
                                    (errVal) -> done.reject(errVal));
        },
        (errVal) -> {
          if (!preventAbort)
            writer.get("abort").asCallable().call(objValue(writer),
                                                  new V6Value[] {errVal});
          done.reject(errVal);
        });
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.defineGetter("locked", (t, a) -> bool(self(t).locked));

    o.set("getReader", fn((t, a) -> {
            V6ReadableStreamObject s = self(t);
            if (s.locked)
              throw new V6Throw(
                  str("TypeError: ReadableStream is already locked"));
            s.locked = true;
            return objValue(buildReader(s));
          }));

    o.set("cancel", fn((t, a) -> {
            V6ReadableStreamObject s = self(t);
            V6Value reason = V6Value.argAt(a, 0);
            s.queue.clear();
            s.closeStream();
            if (s.cancelFn != null)
              return objValue(V6Promise.resolved(
                  s.cancelFn.call(UNDEF, new V6Value[] {reason})));
            return objValue(V6Promise.resolved(UNDEF));
          }));

    o.set(
        "pipeTo", fn((t, a) -> {
          V6ReadableStreamObject s = self(t);
          V6Value destVal = V6Value.argAt(a, 0);
          V6Object dest = (V6Object)destVal.ref();
          V6Value writerVal =
              dest.get("getWriter").asCallable().call(destVal, new V6Value[0]);
          V6Object writer = (V6Object)writerVal.ref();
          boolean preventClose = false;
          boolean preventAbort = false;
          V6Value optsVal = V6Value.argAt(a, 1);
          if (optsVal.tag() == V6Value.TAG_OBJ && optsVal.ref() instanceof
                                                      V6Object) {
            V6Object opts = (V6Object)optsVal.ref();
            preventClose = opts.get("preventClose").truthy();
            preventAbort = opts.get("preventAbort").truthy();
          }
          V6Promise done = new V6Promise();
          pipeChunks(s, writer, done, preventClose, preventAbort);
          return objValue(done);
        }));

    o.set("pipeThrough", fn((t, a) -> {
            V6Value transformVal = V6Value.argAt(a, 0);
            V6Object transform = (V6Object)transformVal.ref();
            V6Value writableVal = transform.get("writable");
            V6Value readableVal = transform.get("readable");
            o.get("pipeTo").asCallable().call(t, new V6Value[] {writableVal});
            return readableVal;
          }));

    o.set("tee", fn((t, a) -> {
            V6ReadableStreamObject s = self(t);
            V6Object src1 = new V6Object();
            V6Object src2 = new V6Object();
            V6ReadableStreamObject branch1 = newStream(objValue(src1));
            V6ReadableStreamObject branch2 = newStream(objValue(src2));
            pumpTee(s, branch1, branch2);
            V6Array result = new V6Array();
            result.push(objValue(branch1));
            result.push(objValue(branch2));
            return objValue(result);
          }));

    return o;
  }

  private static void pumpTee(V6ReadableStreamObject source,
                              V6ReadableStreamObject b1,
                              V6ReadableStreamObject b2) {
    if (b1.closed || b1.errored || b2.closed || b2.errored)
      return;
    source.read().addCallbacks(
        (resultVal)
            -> {
          V6Object result = (V6Object)resultVal.ref();
          if (result.get("done").truthy()) {
            b1.closeStream();
            b2.closeStream();
            return;
          }
          V6Value chunk = result.get("value");
          b1.enqueue(chunk);
          b2.enqueue(chunk);
          pumpTee(source, b1, b2);
        },
        (errVal) -> {
          b1.errorStream(errVal);
          b2.errorStream(errVal);
        });
  }
}

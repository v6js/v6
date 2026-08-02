public final class V6AbortSignalConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();

  public V6AbortSignalConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
    set("abort", fn((thisArg, args) -> {
          V6AbortSignalObject s = newSignal();
          fireAbort(s,
                    args.length > 0 && !args[0].isUndefined()
                        ? args[0]
                        : str("AbortError: signal is aborted without reason"));
          return objValue(s);
        }));
    set("timeout", fn((thisArg, args) -> {
          double ms = V6Value.argAt(args, 0).toNumber();
          V6AbortSignalObject s = newSignal();
          V6EventLoop.schedule((t, a) -> {
            fireAbort(s, str("TimeoutError: signal timed out"));
            return UNDEF;
          }, ms, 0, new V6Value[0]);
          return objValue(s);
        }));
    set("any", fn((thisArg, args) -> {
          V6AbortSignalObject out = newSignal();
          V6Value iterable = V6Value.argAt(args, 0);
          if (iterable.tag() == V6Value.TAG_OBJ && iterable.ref() instanceof
                                                       V6Array) {
            V6Array arr = (V6Array)iterable.ref();
            for (int i = 0; i < arr.elemCount; i++) {
              V6Value sigVal = arr.elements[i];
              if (sigVal.tag() != V6Value.TAG_OBJ ||
                  !(sigVal.ref() instanceof V6AbortSignalObject))
                continue;
              V6AbortSignalObject in = (V6AbortSignalObject)sigVal.ref();
              if (in.aborted) {
                fireAbort(out, in.reason);
                break;
              }
              in.addListener("abort", fn((t, a) -> {
                               if (!out.aborted)
                                 fireAbort(out, in.reason);
                               return UNDEF;
                             }),
                             true, null);
            }
          }
          return objValue(out);
        }));
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

  static V6AbortSignalObject newSignal() {
    V6AbortSignalObject s = new V6AbortSignalObject();
    s.setProto(PROTOTYPE);
    return s;
  }

  static void fireAbort(V6AbortSignalObject s, V6Value reason) {
    if (s.aborted)
      return;
    s.aborted = true;
    s.reason = reason;
    V6EventObject event = new V6EventObject();
    event.setProto(V6EventConstructor.PROTOTYPE);
    event.type = "abort";
    s.dispatch(event);
  }

  @Override
  public V6Value construct(V6Value[] args) {
    throw new V6Throw(str("TypeError: Illegal constructor"));
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6AbortSignalObject self(V6Value t) {
    return (V6AbortSignalObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventTargetConstructor.PROTOTYPE);

    o.defineGetter(
        "aborted",
        (t, a) -> new V6Value(V6Value.TAG_BOOL, self(t).aborted ? 1 : 0, null));
    o.defineGetter("reason", (t, a) -> self(t).reason);

    o.set("throwIfAborted", fn((t, a) -> {
            V6AbortSignalObject s = self(t);
            if (s.aborted)
              throw new V6Throw(s.reason);
            return UNDEF;
          }));

    V6EventHandlerProperty.install(o, "onabort", "abort");

    return o;
  }
}

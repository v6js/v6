public final class V6StreamTransformConstructor
    extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public V6StreamTransformConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
    PROTOTYPE.nativeCtor = this;
  }

  @Override
  public V6Object allocate() {
    V6EventEmitterObject e = new V6EventEmitterObject();
    e.setProto(PROTOTYPE);
    return e;
  }

  @Override
  public V6Value construct(V6Value[] args) {
    return new V6Value(V6Value.TAG_OBJ, 0, allocate());
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();
    o.setProto(V6EventEmitterConstructor.PROTOTYPE);
    V6StreamMethods.installReadable(o);
    V6StreamMethods.installWritable(o);

    o.set("write", fn((t, a) -> {
            V6Value chunk = V6Value.argAt(a, 0);
            V6Callable userCb = V6StreamMethods.lastCallback(a);
            V6Value transformFn = t.getProp("_transform");
            V6Callable pushCb = (t2, a2) -> {
              V6Value err = V6Value.argAt(a2, 0);
              if (err.isUndefined() || err.tag() == V6Value.TAG_NULL) {
                V6Value out = V6Value.argAt(a2, 1);
                if (!out.isUndefined())
                  t.getProp("push").asCallable().call(t, new V6Value[] {out});
              }
              if (userCb != null)
                userCb.call(UNDEF, new V6Value[0]);
              return UNDEF;
            };
            if (transformFn.tag() == V6Value.TAG_FUNC) {
              transformFn.asCallable().call(
                  t,
                  new V6Value[] {chunk, new V6Value(V6Value.TAG_STR, 0, "utf8"),
                                 fn(pushCb)});
            } else {
              t.getProp("push").asCallable().call(t, new V6Value[] {chunk});
              pushCb.call(UNDEF, new V6Value[] {
                                     new V6Value(V6Value.TAG_UNDEF, 0, null)});
            }
            return new V6Value(V6Value.TAG_BOOL, 1, null);
          }));

    o.set("end", fn((t, a) -> {
            if (a.length > 0 && a[0].tag() != V6Value.TAG_FUNC)
              t.getProp("write").asCallable().call(t, new V6Value[] {a[0]});
            t.getProp("push").asCallable().call(
                t, new V6Value[] {new V6Value(V6Value.TAG_NULL, 0, null)});
            t.getProp("emit").asCallable().call(
                t, new V6Value[] {new V6Value(V6Value.TAG_STR, 0, "finish")});
            return UNDEF;
          }));

    return o;
  }
}

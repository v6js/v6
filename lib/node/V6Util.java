public final class V6Util {
  private V6Util() {
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

  private static String formatInternal(V6Value[] args) {
    if (args.length == 0)
      return "";
    String fmt = args[0].toString();
    StringBuilder out = new StringBuilder();
    int argIdx = 1;
    for (int i = 0; i < fmt.length(); i++) {
      char c = fmt.charAt(i);
      if (c == '%' && i + 1 < fmt.length()) {
        char spec = fmt.charAt(i + 1);
        if ((spec == 's' || spec == 'd' || spec == 'i' || spec == 'f' ||
             spec == 'j' || spec == 'o' || spec == 'O' || spec == '%') &&
            (spec == '%' || argIdx < args.length)) {
          i++;
          if (spec == '%') {
            out.append('%');
            continue;
          }
          V6Value arg = args[argIdx++];
          switch (spec) {
          case 's':
            out.append(arg.toString());
            break;
          case 'd':
          case 'i':
            out.append((long)arg.toNumber());
            break;
          case 'f':
            out.append(arg.toNumber());
            break;
          case 'j':
            out.append(V6Json.stringify(arg, UNDEF, UNDEF).toString());
            break;
          case 'o':
          case 'O':
            out.append(
                V6Builtins.inspect(arg, new java.util.IdentityHashMap<>()));
            break;
          }
          continue;
        }
      }
      out.append(c);
    }
    for (; argIdx < args.length; argIdx++) {
      out.append(' ');
      V6Value arg = args[argIdx];
      out.append(
          arg.tag() == V6Value.TAG_STR
              ? arg.toString()
              : V6Builtins.inspect(arg, new java.util.IdentityHashMap<>()));
    }
    return out.toString();
  }

  public static V6Object build() {
    V6Object o = new V6Object();

    o.set("format", fn((thisArg, args) -> str(formatInternal(args))));

    o.set(
        "inspect",
        fn((thisArg, args)
               -> str(V6Builtins.inspect(V6Value.argAt(args, 0),
                                         new java.util.IdentityHashMap<>()))));

    o.set("promisify", fn((thisArg, args) -> {
            V6Callable target = V6Value.argAt(args, 0).asCallable();
            return fn((t, callArgs) -> {
              V6Promise result = new V6Promise();
              V6Value[] fullArgs = new V6Value[callArgs.length + 1];
              System.arraycopy(callArgs, 0, fullArgs, 0, callArgs.length);
              fullArgs[callArgs.length] = fn((cbThis, cbArgs) -> {
                V6Value err = V6Value.argAt(cbArgs, 0);
                if (!err.isUndefined() && err.tag() != V6Value.TAG_NULL) {
                  result.reject(err);
                } else {
                  result.resolve(V6Value.argAt(cbArgs, 1));
                }
                return UNDEF;
              });
              try {
                target.call(t, fullArgs);
              } catch (V6Throw e) {
                result.reject(e.value);
              }
              return objValue(result);
            });
          }));

    o.set("inherits", fn((thisArg, args) -> {
            V6Value ctor = V6Value.argAt(args, 0);
            V6Value superCtor = V6Value.argAt(args, 1);
            V6Object superProtoHolder =
                superCtor.tag() == V6Value.TAG_OBJ ||
                        superCtor.tag() == V6Value.TAG_FUNC
                    ? asObjIfPresent(superCtor)
                    : null;
            V6Value superProto = superProtoHolder != null
                                     ? superProtoHolder.get("prototype")
                                     : UNDEF;
            V6Object newProto = new V6Object();
            if (superProto.tag() == V6Value.TAG_OBJ)
              newProto.setProto((V6Object)superProto.ref());
            newProto.set("constructor", ctor);
            V6Object ctorObj = asObjIfPresent(ctor);
            if (ctorObj != null) {
              ctorObj.set("prototype", objValue(newProto));
              ctorObj.set("super_", superCtor);
            }
            return UNDEF;
          }));

    o.set("types", objValue(buildTypes()));

    o.set("deprecate", fn((thisArg, args) -> {
            V6Callable target = V6Value.argAt(args, 0).asCallable();
            String msg =
                args.length > 1 ? args[1].toString() : "DeprecationWarning";
            boolean[] warned = {false};
            return fn((t, a) -> {
              if (!warned[0]) {
                warned[0] = true;
                System.err.println("(v6:util.deprecate) " + msg);
              }
              return target.call(t, a);
            });
          }));

    o.set("callbackify", fn((thisArg, args) -> {
            V6Callable target = V6Value.argAt(args, 0).asCallable();
            return fn((t, callArgs) -> {
              V6Callable cb = callArgs[callArgs.length - 1].asCallable();
              V6Value[] fnArgs =
                  java.util.Arrays.copyOf(callArgs, callArgs.length - 1);
              V6Value result;
              try {
                result = target.call(t, fnArgs);
              } catch (V6Throw e) {
                cb.call(UNDEF, new V6Value[] {e.value});
                return UNDEF;
              }
              if (result.tag() == V6Value.TAG_OBJ && result.ref() instanceof
                                                         V6Promise) {
                V6Promise p = (V6Promise)result.ref();
                p.addCallbacks(
                    v
                    -> cb.call(UNDEF,
                               new V6Value[] {
                                   new V6Value(V6Value.TAG_NULL, 0, null), v}),
                    err -> cb.call(UNDEF, new V6Value[] {err}));
              } else {
                cb.call(UNDEF,
                        new V6Value[] {new V6Value(V6Value.TAG_NULL, 0, null),
                                       result});
              }
              return UNDEF;
            });
          }));

    return o;
  }

  private static boolean isa(V6Value v, Class<?> cls) {
    return v.tag() == V6Value.TAG_OBJ && cls.isInstance(v.ref());
  }

  private static V6Object buildTypes() {
    V6Object t = new V6Object();
    t.set("isPromise",
          fn((th, a) -> bool(isa(V6Value.argAt(a, 0), V6Promise.class))));
    t.set("isRegExp",
          fn((th, a) -> bool(isa(V6Value.argAt(a, 0), V6Regex.class))));
    t.set("isMap",
          fn((th, a) -> bool(isa(V6Value.argAt(a, 0), V6MapObject.class))));
    t.set("isSet",
          fn((th, a) -> bool(isa(V6Value.argAt(a, 0), V6SetObject.class))));
    t.set("isWeakMap", fn((th, a) -> bool(false)));
    t.set("isWeakSet", fn((th, a) -> bool(false)));
    t.set("isDate", fn((th, a) -> bool(false)));
    t.set(
        "isAsyncFunction",
        fn((th, a)
               -> bool(V6Value.argAt(a, 0).tag() == V6Value.TAG_FUNC &&
                       V6Value.argAt(a, 0).ref() instanceof V6AsyncFunction)));
    t.set("isGeneratorFunction",
          fn((th, a)
                 -> bool(V6Value.argAt(a, 0).tag() == V6Value.TAG_FUNC &&
                         V6Value.argAt(a, 0).ref() instanceof
                             V6GeneratorFunction)));
    t.set("isAsyncGeneratorFunction",
          fn((th, a)
                 -> bool(V6Value.argAt(a, 0).tag() == V6Value.TAG_FUNC &&
                         V6Value.argAt(a, 0).ref() instanceof
                             V6AsyncGeneratorFunction)));
    t.set("isGeneratorObject",
          fn((th, a) -> bool(isa(V6Value.argAt(a, 0), V6Generator.class))));
    return t;
  }

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static V6Object asObjIfPresent(V6Value v) {
    if (v.tag() == V6Value.TAG_OBJ || v.tag() == V6Value.TAG_FUNC)
      return v.ref() instanceof V6Object ? (V6Object)v.ref() : null;
    return null;
  }
}

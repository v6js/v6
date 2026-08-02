import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Method;

public final class V6Closure extends V6Object implements V6Callable {
  private final MethodHandle handle;
  private final V6Ref[] captures;

  public static final V6Object FUNCTION_PROTOTYPE = buildFunctionPrototype();

  private static V6Value fnVal(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6Object buildFunctionPrototype() {
    V6Object o = new V6Object();
    o.set("call", fnVal((t, a) -> {
            V6Value thisArg = V6Value.argAt(a, 0);
            V6Value[] rest = a.length > 0
                                 ? java.util.Arrays.copyOfRange(a, 1, a.length)
                                 : new V6Value[0];
            return t.asCallable().call(thisArg, rest);
          }));
    o.set("apply", fnVal((t, a) -> {
            V6Value thisArg = V6Value.argAt(a, 0);
            V6Value argsArrVal = V6Value.argAt(a, 1);
            V6Value[] callArgs = new V6Value[0];
            if (argsArrVal.tag() == V6Value.TAG_OBJ &&
                argsArrVal.ref() instanceof V6Object) {
              V6Object arr = (V6Object)argsArrVal.ref();
              int n = (int)arr.get("length").num();
              callArgs = new V6Value[n];
              for (int i = 0; i < n; i++)
                callArgs[i] = arr.get(Integer.toString(i));
            }
            return t.asCallable().call(thisArg, callArgs);
          }));
    o.set("bind", fnVal((t, a) -> {
            V6Callable target = t.asCallable();
            V6Value boundThis = V6Value.argAt(a, 0);
            V6Value[] boundArgs =
                a.length > 1 ? java.util.Arrays.copyOfRange(a, 1, a.length)
                             : new V6Value[0];
            V6Callable bound = (callThis, callArgs) -> {
              V6Value[] merged =
                  new V6Value[boundArgs.length + callArgs.length];
              System.arraycopy(boundArgs, 0, merged, 0, boundArgs.length);
              System.arraycopy(callArgs, 0, merged, boundArgs.length,
                               callArgs.length);
              return target.call(boundThis, merged);
            };
            return fnVal(bound);
          }));
    o.set("toString", fnVal((t, a)
                                -> new V6Value(V6Value.TAG_STR, 0,
                                               "function () { [v6 code] }")));
    return o;
  }

  public V6Closure(Class<?> owner, String methodName, V6Ref[] captures) {
    Method found = null;
    for (Method m : owner.getDeclaredMethods()) {
      if (m.getName().equals(methodName)) {
        found = m;
        break;
      }
    }
    found.setAccessible(true);
    try {
      this.handle = MethodHandles.lookup().unreflect(found);
    } catch (IllegalAccessException e) {
      throw new RuntimeException(e);
    }
    this.captures = captures;
    setProto(FUNCTION_PROTOTYPE);
    V6Object proto = new V6Object();
    proto.set("constructor", new V6Value(V6Value.TAG_FUNC, 0, this));
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, proto));
  }

  public MethodHandle handle() {
    return handle;
  }

  public V6Ref[] captures() {
    return captures;
  }

  @Override
  public V6Value call(V6Value thisArg, V6Value[] args) {
    try {
      return (V6Value)handle.invokeExact(captures, thisArg, args);
    } catch (RuntimeException | Error e) {
      throw e;
    } catch (Throwable t) {
      throw new RuntimeException(t);
    }
  }

  @Override
  public String toString() {
    return "function () { [v6 code] }";
  }
}

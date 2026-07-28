import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.reflect.Method;

public final class V6Closure implements V6Callable {
  private final MethodHandle handle;
  private final V6Ref[] captures;

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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public final class V6Closure implements V6Callable {
  private final Method method;
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
    this.method = found;
    this.captures = captures;
  }

  @Override
  public V6Value call(V6Value thisArg, V6Value[] args) {
    try {
      return (V6Value)method.invoke(null, captures, thisArg, args);
    } catch (InvocationTargetException e) {
      if (e.getCause() instanceof RuntimeException)
        throw (RuntimeException)e.getCause();
      throw new RuntimeException(e.getCause());
    } catch (ReflectiveOperationException e) {
      throw new RuntimeException(e);
    }
  }

  @Override
  public String toString() {
    return "function () { [v6 code] }";
  }
}

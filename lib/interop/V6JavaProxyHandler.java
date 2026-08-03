import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;

public final class V6JavaProxyHandler implements InvocationHandler {
  private final V6Value jsTarget;

  V6JavaProxyHandler(V6Value jsTarget) {
    this.jsTarget = jsTarget;
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  @Override
  public Object invoke(Object proxy, Method method, Object[] args) {
    String name = method.getName();
    int argc = args == null ? 0 : args.length;

    if (name.equals("toString") && argc == 0)
      return "[JS Proxy]";
    if (name.equals("hashCode") && argc == 0)
      return System.identityHashCode(proxy);
    if (name.equals("equals") && argc == 1)
      return proxy == args[0];

    V6Callable callable;
    if (jsTarget.tag() == V6Value.TAG_FUNC) {
      callable = jsTarget.asCallable();
    } else {
      V6Value member = jsTarget.getProp(name);
      if (member.tag() != V6Value.TAG_FUNC)
        return V6JavaMarshal.toJava(
            V6JavaMarshal.defaultReturnFor(method.getReturnType()),
            method.getReturnType());
      callable = member.asCallable();
    }

    V6Value[] jsArgs = new V6Value[argc];
    for (int i = 0; i < argc; i++)
      jsArgs[i] = V6JavaMarshal.toJs(args[i]);

    if (V6JavaInterop.onMainThread() || !V6EventLoop.hasStarted()) {
      V6Value result = callable.call(UNDEF, jsArgs);
      return V6JavaMarshal.toJava(result, method.getReturnType());
    }

    boolean isVoid = method.getReturnType() == void.class ||
                     method.getReturnType() == Void.class;
    if (isVoid) {
      V6EventLoop.postExternal(() -> callable.call(UNDEF, jsArgs));
      return null;
    }

    CountDownLatch latch = new CountDownLatch(1);
    Object[] resultHolder = new Object[1];
    RuntimeException[] errHolder = new RuntimeException[1];
    V6EventLoop.postExternal(() -> {
      try {
        V6Value result = callable.call(UNDEF, jsArgs);
        resultHolder[0] = V6JavaMarshal.toJava(result, method.getReturnType());
      } catch (V6Throw t) {
        errHolder[0] = new RuntimeException(String.valueOf(t.value));
      } catch (RuntimeException t) {
        errHolder[0] = t;
      } finally {
        latch.countDown();
      }
    });
    try {
      latch.await();
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
    }
    if (errHolder[0] != null)
      throw errHolder[0];
    return resultHolder[0];
  }
}

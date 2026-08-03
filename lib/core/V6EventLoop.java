import java.util.HashMap;
import java.util.Map;
import java.util.PriorityQueue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;

public final class V6EventLoop {
  private V6EventLoop() {
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);
  private static final PriorityQueue<V6TimerTask> timers =
      new PriorityQueue<>();
  private static final Map<Long, V6TimerTask> byId = new HashMap<>();
  private static final ConcurrentLinkedQueue<Runnable> external =
      new ConcurrentLinkedQueue<>();
  private static final AtomicInteger refCount = new AtomicInteger(0);
  private static long nextId = 1;

  public static synchronized long schedule(V6Callable cb, double delayMs,
                                           double intervalMs, V6Value[] args) {
    V6TimerTask t = new V6TimerTask();
    t.id = nextId++;
    t.fireAt = System.currentTimeMillis() + (long)Math.max(0, delayMs);
    t.interval = (long)Math.max(0, intervalMs);
    t.callback = cb;
    t.args = args;
    timers.add(t);
    byId.put(t.id, t);
    return t.id;
  }

  public static synchronized void cancel(long id) {
    V6TimerTask t = byId.remove(id);
    if (t != null)
      t.cancelled = true;
  }

  public static void ref() {
    refCount.incrementAndGet();
  }

  public static void unref() {
    refCount.decrementAndGet();
  }

  public static void postExternal(Runnable r) {
    external.add(r);
  }

  private static boolean hasOtherNonDaemonThreads() {
    ThreadGroup root = Thread.currentThread().getThreadGroup();
    while (root.getParent() != null)
      root = root.getParent();
    Thread[] threads = new Thread[root.activeCount() + 16];
    int n = root.enumerate(threads, true);
    Thread me = Thread.currentThread();
    for (int i = 0; i < n; i++) {
      Thread th = threads[i];
      if (th != null && th != me && th.isAlive() && !th.isDaemon())
        return true;
    }
    return false;
  }

  private static void sleepQuiet(long ms) {
    if (ms <= 0)
      return;
    try {
      Thread.sleep(ms);
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
    }
  }

  private static void runGuarded(Runnable r) {
    try {
      r.run();
    } catch (V6Throw e) {
      if (!V6Process.dispatchUncaught(e.value))
        throw e;
    } catch (RuntimeException e) {
      V6Value wrapped =
          new V6Value(V6Value.TAG_STR, 0, String.valueOf(e.getMessage()));
      if (!V6Process.dispatchUncaught(wrapped))
        throw e;
    }
  }

  private static volatile boolean started = false;

  public static boolean hasStarted() {
    return started;
  }

  public static void run() {
    started = true;
    V6MicrotaskQueue.drain();
    while (true) {
      Runnable ext;
      while ((ext = external.poll()) != null) {
        runGuarded(ext);
        V6MicrotaskQueue.drain();
      }

      V6TimerTask t;
      synchronized (V6EventLoop.class) {
        t = timers.peek();
      }
      if (t == null) {
        if (refCount.get() > 0 || hasOtherNonDaemonThreads()) {
          sleepQuiet(15);
          continue;
        }
        break;
      }
      long now = System.currentTimeMillis();
      if (t.fireAt > now) {
        sleepQuiet(Math.min(t.fireAt - now, 20));
        continue;
      }
      synchronized (V6EventLoop.class) {
        timers.poll();
      }
      if (t.cancelled)
        continue;
      if (t.interval > 0) {
        synchronized (V6EventLoop.class) {
          if (!t.cancelled) {
            t.fireAt = now + t.interval;
            timers.add(t);
          }
        }
      } else {
        synchronized (V6EventLoop.class) {
          byId.remove(t.id);
        }
      }
      final V6TimerTask ft = t;
      runGuarded(() -> ft.callback.call(UNDEF, ft.args));
      V6MicrotaskQueue.drain();
    }
    V6Process.dispatchExit(0);
  }
}

public final class V6EventLoop {
  private V6EventLoop() {
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);
  private static final InheritableThreadLocal<V6EventLoopState> STATE =
      new InheritableThreadLocal<>();
  private static volatile boolean globalStarted = false;
  private static volatile V6EventLoopState activeState = null;

  public static void resetForThread() {
    STATE.set(new V6EventLoopState());
    globalStarted = false;
  }

  public static void clearForThread() {
    STATE.remove();
  }

  private static V6EventLoopState state() {
    V6EventLoopState s = STATE.get();
    if (s == null) {
      V6EventLoopState active = activeState;
      if (active != null)
        return active;
      s = new V6EventLoopState();
      STATE.set(s);
    }
    return s;
  }

  public static long schedule(V6Callable cb, double delayMs, double intervalMs,
                              V6Value[] args) {
    V6EventLoopState s = state();
    synchronized (s) {
      V6TimerTask t = new V6TimerTask();
      t.id = s.nextId++;
      t.fireAt = System.currentTimeMillis() + (long)Math.max(0, delayMs);
      t.interval = (long)Math.max(0, intervalMs);
      t.callback = cb;
      t.args = args;
      s.timers.add(t);
      s.byId.put(t.id, t);
      return t.id;
    }
  }

  public static void cancel(long id) {
    V6EventLoopState s = state();
    synchronized (s) {
      V6TimerTask t = s.byId.remove(id);
      if (t != null)
        t.cancelled = true;
    }
  }

  public static void ref() {
    state().refCount.incrementAndGet();
  }

  public static void unref() {
    state().refCount.decrementAndGet();
  }

  public static void postExternal(Runnable r) {
    state().external.add(r);
  }

  public static Object captureState() {
    return state();
  }

  public static void postExternalTo(Object capturedState, Runnable r) {
    ((V6EventLoopState)capturedState).external.add(r);
  }

  public static void refCaptured(Object capturedState) {
    ((V6EventLoopState)capturedState).refCount.incrementAndGet();
  }

  public static void unrefCaptured(Object capturedState) {
    ((V6EventLoopState)capturedState).refCount.decrementAndGet();
  }

  public static void reset() {
    resetForThread();
  }

  private static volatile Thread ignoredThread = null;

  public static void setIgnoredThread(Thread t) {
    ignoredThread = t;
  }

  private static boolean hasOtherNonDaemonThreads() {
    ThreadGroup root = Thread.currentThread().getThreadGroup();
    while (root.getParent() != null)
      root = root.getParent();
    Thread[] threads = new Thread[root.activeCount() + 16];
    int n = root.enumerate(threads, true);
    Thread me = Thread.currentThread();
    Thread ignored = ignoredThread;
    for (int i = 0; i < n; i++) {
      Thread th = threads[i];
      if (th != null && th != me && th != ignored && th.isAlive() &&
          !th.isDaemon())
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

  public static boolean hasStarted() {
    return globalStarted;
  }

  public static void run() {
    V6EventLoopState s = state();
    activeState = s;
    globalStarted = true;
    V6MicrotaskQueue.drain();
    while (true) {
      Runnable ext;
      while ((ext = s.external.poll()) != null) {
        runGuarded(ext);
        V6MicrotaskQueue.drain();
      }

      V6TimerTask t;
      synchronized (s) {
        t = s.timers.peek();
      }
      if (t == null) {
        if (s.refCount.get() > 0 || hasOtherNonDaemonThreads()) {
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
      synchronized (s) {
        s.timers.poll();
      }
      if (t.cancelled)
        continue;
      if (t.interval > 0) {
        synchronized (s) {
          if (!t.cancelled) {
            t.fireAt = now + t.interval;
            s.timers.add(t);
          }
        }
      } else {
        synchronized (s) {
          s.byId.remove(t.id);
        }
      }
      final V6TimerTask ft = t;
      runGuarded(() -> ft.callback.call(UNDEF, ft.args));
      V6MicrotaskQueue.drain();
    }
    activeState = null;
    V6Process.dispatchExit(0);
  }
}

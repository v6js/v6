import java.util.ArrayDeque;

public final class V6MicrotaskQueue {
  private static final InheritableThreadLocal<ArrayDeque<Runnable>> QUEUE =
      new InheritableThreadLocal<>();

  private static ArrayDeque<Runnable> queue() {
    ArrayDeque<Runnable> q = QUEUE.get();
    if (q == null) {
      q = new ArrayDeque<>();
      QUEUE.set(q);
    }
    return q;
  }

  public static void enqueue(Runnable r) {
    synchronized (queue()) {
      queue().add(r);
    }
  }

  public static void drain() {
    ArrayDeque<Runnable> q = queue();
    while (true) {
      Runnable r;
      synchronized (q) {
        r = q.poll();
      }
      if (r == null)
        break;
      r.run();
    }
  }

  public static void reset() {
    QUEUE.set(new ArrayDeque<>());
  }
}

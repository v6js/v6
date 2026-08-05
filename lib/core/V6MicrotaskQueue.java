import java.util.ArrayDeque;

public final class V6MicrotaskQueue {
  private static final ArrayDeque<Runnable> queue = new ArrayDeque<>();

  public static void enqueue(Runnable r) {
    queue.add(r);
  }

  public static void drain() {
    while (!queue.isEmpty()) {
      Runnable r = queue.poll();
      r.run();
    }
  }

  public static void reset() {
    queue.clear();
  }
}

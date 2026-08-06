import java.util.HashMap;
import java.util.Map;
import java.util.PriorityQueue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicInteger;

public final class V6EventLoopState {
  final PriorityQueue<V6TimerTask> timers = new PriorityQueue<>();
  final Map<Long, V6TimerTask> byId = new HashMap<>();
  final ConcurrentLinkedQueue<Runnable> external =
      new ConcurrentLinkedQueue<>();
  final AtomicInteger refCount = new AtomicInteger(0);
  long nextId = 1;
  volatile boolean started = false;
}

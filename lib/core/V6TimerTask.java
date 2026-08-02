public final class V6TimerTask implements Comparable<V6TimerTask> {
  public long fireAt;
  public long interval;
  public long id;
  public V6Callable callback;
  public V6Value[] args;
  public boolean cancelled;

  @Override
  public int compareTo(V6TimerTask o) {
    return Long.compare(fireAt, o.fireAt);
  }
}

public class V6EventObject extends V6Object {
  String type = "";
  V6Value target = new V6Value(V6Value.TAG_NULL, 0, null);
  V6Value currentTarget = new V6Value(V6Value.TAG_NULL, 0, null);
  boolean bubbles = false;
  boolean cancelable = false;
  boolean composed = false;
  boolean defaultPrevented = false;
  boolean propagationStopped = false;
  boolean immediatePropagationStopped = false;
  final double timeStamp = System.nanoTime() / 1_000_000.0;
}

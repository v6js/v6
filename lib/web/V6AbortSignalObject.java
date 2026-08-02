public final class V6AbortSignalObject extends V6EventTargetObject {
  boolean aborted = false;
  V6Value reason = new V6Value(V6Value.TAG_UNDEF, 0, null);
}

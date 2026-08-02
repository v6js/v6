public final class V6EventHandlerProperty {
  private V6EventHandlerProperty() {
  }

  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public static void install(V6Object proto, String propName,
                             String eventType) {
    proto.defineGetter(propName, (t, a) -> {
      V6EventTargetObject e = (V6EventTargetObject)t.ref();
      V6Value v = e.handlerRaw.get(eventType);
      return v != null ? v : UNDEF;
    });
    proto.defineSetter(propName, (t, a) -> {
      V6EventTargetObject e = (V6EventTargetObject)t.ref();
      V6Value old = e.handlerRaw.get(eventType);
      if (old != null)
        e.removeListener(eventType, old);
      V6Value newVal = V6Value.argAt(a, 0);
      if (newVal.tag() == V6Value.TAG_FUNC) {
        e.handlerRaw.put(eventType, newVal);
        e.addListener(eventType, newVal, false, null);
      } else {
        e.handlerRaw.remove(eventType);
      }
      return UNDEF;
    });
  }
}

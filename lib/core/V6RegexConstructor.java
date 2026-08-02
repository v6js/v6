public final class V6RegexConstructor
    extends V6Object implements V6NativeConstructor {
  @Override
  public V6Value construct(V6Value[] args) {
    V6Value first = V6Value.argAt(args, 0);
    V6Value second = V6Value.argAt(args, 1);
    String source;
    String flags;
    if (first.tag() == V6Value.TAG_OBJ && first.ref() instanceof V6Regex) {
      V6Regex re = (V6Regex)first.ref();
      source = re.get("source").toString();
      flags =
          second.isUndefined() ? re.get("flags").toString() : second.toString();
    } else {
      source = first.isUndefined() ? "" : first.toString();
      flags = second.isUndefined() ? "" : second.toString();
    }
    return new V6Value(V6Value.TAG_OBJ, 0, new V6Regex(source, flags));
  }

  @Override
  public V6Object prototypeObject() {
    return V6Builtins.REGEXP_PROTOTYPE;
  }
}

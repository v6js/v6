final class V6GeneratorReturn extends RuntimeException {
  final V6Value value;

  V6GeneratorReturn(V6Value value) {
    super(null, null, false, false);
    this.value = value;
  }
}

public final class V6Symbol {
  public final String description;

  public V6Symbol(String description) {
    this.description = description;
  }

  @Override
  public String toString() {
    return "Symbol(" + (description != null ? description : "") + ")";
  }
}

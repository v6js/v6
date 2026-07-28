import java.util.ArrayDeque;

public final class V6Rope implements CharSequence {
  private CharSequence left;
  private CharSequence right;
  private String flat;
  private final int length;

  public V6Rope(CharSequence left, CharSequence right) {
    this.left = left;
    this.right = right;
    this.length = left.length() + right.length();
  }

  @Override
  public int length() {
    return length;
  }

  @Override
  public char charAt(int index) {
    return flatten().charAt(index);
  }

  @Override
  public CharSequence subSequence(int start, int end) {
    return flatten().subSequence(start, end);
  }

  @Override
  public String toString() {
    return flatten();
  }

  private String flatten() {
    if (flat != null)
      return flat;
    StringBuilder sb = new StringBuilder(length);
    ArrayDeque<CharSequence> stack = new ArrayDeque<>();
    stack.push(right);
    stack.push(left);
    while (!stack.isEmpty()) {
      CharSequence cs = stack.pop();
      if (cs instanceof V6Rope r && r.flat == null) {
        stack.push(r.right);
        stack.push(r.left);
      } else {
        sb.append(cs.toString());
      }
    }
    flat = sb.toString();
    left = null;
    right = null;
    return flat;
  }
}

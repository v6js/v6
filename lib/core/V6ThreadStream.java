import java.io.IOException;
import java.io.OutputStream;

public final class V6ThreadStream extends OutputStream {
  private final InheritableThreadLocal<OutputStream> target =
      new InheritableThreadLocal<>();
  private final OutputStream fallback;

  public V6ThreadStream(OutputStream fallback) {
    this.fallback = fallback;
  }

  public void bind(OutputStream out) {
    target.set(out);
  }

  public void unbind() {
    target.remove();
  }

  private OutputStream current() {
    OutputStream t = target.get();
    return t != null ? t : fallback;
  }

  @Override
  public void write(int b) throws IOException {
    current().write(b);
  }

  @Override
  public void write(byte[] b, int off, int len) throws IOException {
    current().write(b, off, len);
  }

  @Override
  public void flush() throws IOException {
    current().flush();
  }
}

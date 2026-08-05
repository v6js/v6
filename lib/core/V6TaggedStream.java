import java.io.DataOutputStream;
import java.io.IOException;
import java.io.OutputStream;

public final class V6TaggedStream extends OutputStream {
  private final DataOutputStream out;
  private final byte tag;
  private final Object lock;

  public V6TaggedStream(DataOutputStream out, byte tag, Object lock) {
    this.out = out;
    this.tag = tag;
    this.lock = lock;
  }

  @Override
  public void write(int b) throws IOException {
    write(new byte[] {(byte)b}, 0, 1);
  }

  @Override
  public void write(byte[] b, int off, int len) throws IOException {
    if (len == 0)
      return;
    synchronized (lock) {
      out.writeByte(tag);
      out.writeInt(len);
      out.write(b, off, len);
      out.flush();
    }
  }
}

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;

public final class V6BodyUtil {
  private V6BodyUtil() {
  }

  public static Object[] normalize(V6Value bodyVal) {
    if (bodyVal.isUndefined() || bodyVal.tag() == V6Value.TAG_NULL)
      return new Object[] {new byte[0], null};

    if (bodyVal.tag() == V6Value.TAG_STR)
      return new Object[] {bodyVal.toString().getBytes(StandardCharsets.UTF_8),
                           "text/plain;charset=UTF-8"};

    if (bodyVal.tag() == V6Value.TAG_OBJ) {
      Object ref = bodyVal.ref();
      if (ref instanceof V6BlobObject) {
        V6BlobObject b = (V6BlobObject)ref;
        return new Object[] {b.data, b.type.isEmpty() ? null : b.type};
      }
      if (ref instanceof V6Buffer)
        return new Object[] {((V6Buffer)ref).toBytes(), null};
      if (ref instanceof V6ArrayBufferObject)
        return new Object[] {((V6ArrayBufferObject)ref).data, null};
      if (ref instanceof V6UrlSearchParamsObject) {
        String s = V6UrlSearchParamsConstructor.stringify(
            (V6UrlSearchParamsObject)ref);
        return new Object[] {s.getBytes(StandardCharsets.UTF_8),
                             "application/x-www-form-urlencoded;charset=UTF-8"};
      }
      if (ref instanceof V6FormDataObject)
        return multipartEncode((V6FormDataObject)ref);
    }
    return new Object[] {bodyVal.toString().getBytes(StandardCharsets.UTF_8),
                         "text/plain;charset=UTF-8"};
  }

  private static Object[] multipartEncode(V6FormDataObject fd) {
    String boundary = "----v6FormBoundary" + System.nanoTime();
    ByteArrayOutputStream out = new ByteArrayOutputStream();
    try {
      for (int i = 0; i < fd.keys.size(); i++) {
        String name = fd.keys.get(i);
        V6Value value = fd.values.get(i);
        out.write(("--" + boundary + "\r\n").getBytes(StandardCharsets.UTF_8));
        if (value.tag() == V6Value.TAG_OBJ && value.ref() instanceof
                                                  V6BlobObject) {
          V6BlobObject blob = (V6BlobObject)value.ref();
          String filename =
              blob instanceof V6FileObject ? ((V6FileObject)blob).name : "blob";
          out.write(("Content-Disposition: form-data; name=\"" + name +
                     "\"; filename=\"" + filename + "\"\r\n")
                        .getBytes(StandardCharsets.UTF_8));
          String ct =
              blob.type.isEmpty() ? "application/octet-stream" : blob.type;
          out.write(("Content-Type: " + ct + "\r\n\r\n")
                        .getBytes(StandardCharsets.UTF_8));
          out.write(blob.data);
          out.write("\r\n".getBytes(StandardCharsets.UTF_8));
        } else {
          out.write(
              ("Content-Disposition: form-data; name=\"" + name + "\"\r\n\r\n")
                  .getBytes(StandardCharsets.UTF_8));
          out.write(value.toString().getBytes(StandardCharsets.UTF_8));
          out.write("\r\n".getBytes(StandardCharsets.UTF_8));
        }
      }
      out.write(("--" + boundary + "--\r\n").getBytes(StandardCharsets.UTF_8));
    } catch (java.io.IOException ignored) {
    }
    return new Object[] {out.toByteArray(),
                         "multipart/form-data; boundary=" + boundary};
  }
}

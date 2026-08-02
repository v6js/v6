import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class V6UrlObject extends V6Object {
  String protocol = "";
  String username = "";
  String password = "";
  String hostname = "";
  String port = "";
  String pathname = "";
  String hash = "";
  V6UrlSearchParamsObject searchParams;

  private static final Pattern URL_PATTERN = Pattern.compile(
      "^([a-zA-Z][a-zA-Z0-9+.-]*):(?://(?:([^:@/]*)(?::([^@/]*))?@)?([^:/"
      + "?#]*)(?::(\\d+))?)?([^?#]*)(?:\\?([^#]*))?(?:#(.*))?$");

  V6UrlObject(String href, String base) {
    Matcher m = URL_PATTERN.matcher(href);
    if (!m.matches() && base != null) {
      try {
        String resolved = new java.net.URI(base).resolve(href).toString();
        m = URL_PATTERN.matcher(resolved);
      } catch (Exception ignored) {
      }
    }
    if (!m.matches())
      throw new V6Throw(
          new V6Value(V6Value.TAG_STR, 0,
                      "TypeError [ERR_INVALID_URL]: Invalid URL: " + href));
    protocol = m.group(1) + ":";
    username = m.group(2) != null ? m.group(2) : "";
    password = m.group(3) != null ? m.group(3) : "";
    hostname = m.group(4) != null ? m.group(4) : "";
    port = m.group(5) != null ? m.group(5) : "";
    pathname = m.group(6) != null ? m.group(6) : "";
    String search = m.group(7) != null ? m.group(7) : "";
    hash = m.group(8) != null ? m.group(8) : "";
    if (pathname.isEmpty() && !hostname.isEmpty())
      pathname = "/";
    searchParams = V6UrlSearchParamsConstructor.parseInto(search);
  }

  String search() {
    String s = V6UrlSearchParamsConstructor.stringify(searchParams);
    return s.isEmpty() ? "" : "?" + s;
  }

  String host() {
    return port.isEmpty() ? hostname : hostname + ":" + port;
  }

  String origin() {
    return protocol + "//" + host();
  }

  String href() {
    StringBuilder sb = new StringBuilder();
    sb.append(protocol).append("//");
    if (!username.isEmpty()) {
      sb.append(username);
      if (!password.isEmpty())
        sb.append(':').append(password);
      sb.append('@');
    }
    sb.append(host());
    sb.append(pathname);
    sb.append(search());
    if (!hash.isEmpty())
      sb.append('#').append(hash);
    return sb.toString();
  }
}

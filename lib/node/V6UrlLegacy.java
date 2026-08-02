import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class V6UrlLegacy {
  private V6UrlLegacy() {
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value objValue(V6Object o) {
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  private static final V6Value NUL = new V6Value(V6Value.TAG_NULL, 0, null);
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  private static V6Value bool(boolean b) {
    return new V6Value(V6Value.TAG_BOOL, b ? 1 : 0, null);
  }

  private static final Pattern LEGACY_PATTERN = Pattern.compile(
      "^(?:([a-zA-Z][a-zA-Z0-9+.-]*):)?(//(?:([^:@/]*)(?::([^@/]*))?@)?([^:/" +
      "?#]*)(?::(\\d+))?)?([^?#]*)(?:\\?([^#]*))?(?:#(.*))?$");

  static V6Value parseImpl(V6Value[] args) {
    String urlStr = V6Value.argAt(args, 0).toString();
    boolean parseQueryString = args.length > 1 && args[1].truthy();

    Matcher m = LEGACY_PATTERN.matcher(urlStr);
    V6Object result = new V6Object();
    if (!m.matches()) {
      result.set("href", str(urlStr));
      result.set("pathname", str(urlStr));
      result.set("path", str(urlStr));
      result.set("protocol", NUL);
      result.set("slashes", NUL);
      result.set("auth", NUL);
      result.set("host", NUL);
      result.set("port", NUL);
      result.set("hostname", NUL);
      result.set("hash", NUL);
      result.set("search", NUL);
      result.set("query", parseQueryString ? objValue(new V6Object()) : NUL);
      return objValue(result);
    }

    String protocol = m.group(1);
    boolean slashes = m.group(2) != null;
    String user = m.group(3);
    String pass = m.group(4);
    String hostname =
        m.group(5) != null && !m.group(5).isEmpty() ? m.group(5) : null;
    String port = m.group(6);
    String pathnameRaw = m.group(7);
    String query = m.group(8);
    String hash = m.group(9);

    String auth = null;
    if (user != null) {
      auth = pass != null ? user + ":" + pass : user;
    }
    String pathname = pathnameRaw != null && !pathnameRaw.isEmpty()
                          ? pathnameRaw
                          : (hostname != null ? "/" : null);

    result.set("protocol", protocol != null ? str(protocol + ":") : NUL);
    result.set("slashes",
               slashes ? bool(true) : (protocol != null ? bool(false) : NUL));
    result.set("auth", auth != null ? str(auth) : NUL);
    result.set("hostname", hostname != null ? str(hostname) : NUL);
    result.set("port", port != null ? str(port) : NUL);
    result.set("host",
               hostname != null
                   ? str(port != null ? hostname + ":" + port : hostname)
                   : NUL);
    result.set("hash", hash != null ? str("#" + hash) : NUL);
    result.set("search", query != null ? str("?" + query) : NUL);
    result.set("query", parseQueryString
                            ? objValue(V6QueryString.parseToObject(
                                  query != null ? query : "", "&", "="))
                            : (query != null ? str(query) : NUL));
    result.set("pathname", pathname != null ? str(pathname) : NUL);
    String path =
        (pathname != null ? pathname : "") + (query != null ? "?" + query : "");
    result.set("path", path.isEmpty() ? NUL : str(path));

    StringBuilder href = new StringBuilder();
    if (protocol != null)
      href.append(protocol).append(':');
    if (slashes)
      href.append("//");
    if (auth != null)
      href.append(auth).append('@');
    if (hostname != null)
      href.append(hostname);
    if (port != null)
      href.append(':').append(port);
    if (pathname != null)
      href.append(pathname);
    if (query != null)
      href.append('?').append(query);
    if (hash != null)
      href.append('#').append(hash);
    result.set("href", str(href.toString()));

    return objValue(result);
  }

  private static String stringFromField(V6Object o, String key) {
    V6Value v = o.get(key);
    return v.tag() == V6Value.TAG_STR ? v.toString() : null;
  }

  static V6Value formatImpl(V6Value[] args) {
    V6Value arg = V6Value.argAt(args, 0);
    if (arg.tag() == V6Value.TAG_STR)
      return arg;
    if (arg.tag() != V6Value.TAG_OBJ || !(arg.ref() instanceof V6Object))
      throw new V6Throw(str("TypeError [ERR_INVALID_ARG_TYPE]: url.format " +
                            "expects a string or object"));

    if (arg.ref() instanceof V6UrlObject)
      return str(((V6UrlObject)arg.ref()).href());

    V6Object o = (V6Object)arg.ref();
    String protocol = stringFromField(o, "protocol");
    String hostname = stringFromField(o, "hostname");
    String host = stringFromField(o, "host");
    String port = stringFromField(o, "port");
    String auth = stringFromField(o, "auth");
    String pathname = stringFromField(o, "pathname");
    String hash = stringFromField(o, "hash");
    V6Value slashesVal = o.get("slashes");
    boolean slashes = slashesVal.truthy();

    String search = stringFromField(o, "search");
    if (search == null) {
      V6Value queryVal = o.get("query");
      if (queryVal.tag() == V6Value.TAG_STR && !queryVal.toString().isEmpty())
        search = "?" + queryVal.toString();
      else if (queryVal.tag() == V6Value.TAG_OBJ && queryVal.ref() instanceof
                                                        V6Object) {
        String qs = V6QueryString.build()
                        .get("stringify")
                        .asCallable()
                        .call(UNDEF, new V6Value[] {queryVal})
                        .toString();
        search = qs.isEmpty() ? "" : "?" + qs;
      }
    }

    StringBuilder sb = new StringBuilder();
    if (protocol != null) {
      sb.append(protocol);
      if (!protocol.endsWith(":"))
        sb.append(':');
    }
    String hostPart = host != null ? host : hostname;
    if (hostPart != null || slashes)
      sb.append("//");
    if (auth != null)
      sb.append(auth).append('@');
    if (hostPart != null) {
      sb.append(hostPart);
      if (host == null && port != null)
        sb.append(':').append(port);
    }
    if (pathname != null)
      sb.append(pathname);
    if (search != null)
      sb.append(search);
    if (hash != null)
      sb.append(hash.startsWith("#") ? hash : "#" + hash);
    return str(sb.toString());
  }

  static V6Value resolveImpl(V6Value[] args) {
    String from = V6Value.argAt(args, 0).toString();
    String to = V6Value.argAt(args, 1).toString();
    try {
      V6UrlObject resolved = new V6UrlObject(to, from);
      return str(resolved.href());
    } catch (V6Throw e) {
      throw new V6Throw(str("TypeError [ERR_INVALID_URL]: Invalid URL"));
    }
  }
}

public final class V6UrlConstructor extends V6Object implements V6NativeConstructor {
  public static final V6Object PROTOTYPE = buildPrototype();
  private static final V6Value UNDEF = new V6Value(V6Value.TAG_UNDEF, 0, null);

  public V6UrlConstructor() {
    set("prototype", new V6Value(V6Value.TAG_OBJ, 0, PROTOTYPE));
  }

  @Override
  public V6Value construct(V6Value[] args) {
    String href = V6Value.argAt(args, 0).toString();
    String base =
        args.length > 1 && !args[1].isUndefined() ? args[1].toString() : null;
    V6UrlObject o = new V6UrlObject(href, base);
    o.setProto(PROTOTYPE);
    return new V6Value(V6Value.TAG_OBJ, 0, o);
  }

  @Override
  public V6Object prototypeObject() {
    return PROTOTYPE;
  }

  private static V6Value str(String s) {
    return new V6Value(V6Value.TAG_STR, 0, s);
  }

  private static V6Value fn(V6Callable c) {
    return new V6Value(V6Value.TAG_FUNC, 0, c);
  }

  private static V6UrlObject self(V6Value t) {
    return (V6UrlObject)t.ref();
  }

  private static V6Object buildPrototype() {
    V6Object o = new V6Object();

    o.defineGetter("href", (t, a) -> str(self(t).href()));
    o.defineSetter("href", (t, a) -> {
      V6UrlObject cur = self(t);
      V6UrlObject fresh = new V6UrlObject(V6Value.argAt(a, 0).toString(), null);
      cur.protocol = fresh.protocol;
      cur.username = fresh.username;
      cur.password = fresh.password;
      cur.hostname = fresh.hostname;
      cur.port = fresh.port;
      cur.pathname = fresh.pathname;
      cur.hash = fresh.hash;
      cur.searchParams = fresh.searchParams;
      return UNDEF;
    });

    o.defineGetter("protocol", (t, a) -> str(self(t).protocol));
    o.defineSetter("protocol", (t, a) -> {
      String p = V6Value.argAt(a, 0).toString();
      self(t).protocol = p.endsWith(":") ? p : p + ":";
      return UNDEF;
    });

    o.defineGetter("username", (t, a) -> str(self(t).username));
    o.defineSetter("username", (t, a) -> {
      self(t).username = V6Value.argAt(a, 0).toString();
      return UNDEF;
    });

    o.defineGetter("password", (t, a) -> str(self(t).password));
    o.defineSetter("password", (t, a) -> {
      self(t).password = V6Value.argAt(a, 0).toString();
      return UNDEF;
    });

    o.defineGetter("hostname", (t, a) -> str(self(t).hostname));
    o.defineSetter("hostname", (t, a) -> {
      self(t).hostname = V6Value.argAt(a, 0).toString();
      return UNDEF;
    });

    o.defineGetter("port", (t, a) -> str(self(t).port));
    o.defineSetter("port", (t, a) -> {
      self(t).port = V6Value.argAt(a, 0).toString();
      return UNDEF;
    });

    o.defineGetter("host", (t, a) -> str(self(t).host()));

    o.defineGetter("pathname", (t, a) -> str(self(t).pathname));
    o.defineSetter("pathname", (t, a) -> {
      self(t).pathname = V6Value.argAt(a, 0).toString();
      return UNDEF;
    });

    o.defineGetter("search", (t, a) -> str(self(t).search()));
    o.defineSetter("search", (t, a) -> {
      V6UrlObject u = self(t);
      String q = V6Value.argAt(a, 0).toString();
      V6UrlSearchParamsObject sp = V6UrlSearchParamsConstructor.parseInto(q);
      u.searchParams.pairs.clear();
      u.searchParams.pairs.addAll(sp.pairs);
      return UNDEF;
    });

    o.defineGetter("hash", (t, a) -> {
      String h = self(t).hash;
      return str(h.isEmpty() ? "" : "#" + h);
    });
    o.defineSetter("hash", (t, a) -> {
      String h = V6Value.argAt(a, 0).toString();
      self(t).hash = h.startsWith("#") ? h.substring(1) : h;
      return UNDEF;
    });

    o.defineGetter("origin", (t, a) -> str(self(t).origin()));
    o.defineGetter("searchParams",
                   (t, a) -> new V6Value(V6Value.TAG_OBJ, 0, self(t).searchParams));

    o.set("toString", fn((t, a) -> str(self(t).href())));
    o.set("toJSON", fn((t, a) -> str(self(t).href())));

    return o;
  }
}

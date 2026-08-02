const u = new URL("https://user:pass@example.com:8080/a/b?x=1&y=2#frag");
console.log(u.protocol);
console.log(u.username, u.password);
console.log(u.hostname, u.port, u.host);
console.log(u.pathname);
console.log(u.search);
console.log(u.hash);
console.log(u.origin);
console.log(u.href);

u.searchParams.set("z", "3");
console.log(u.search);
console.log(u.href);

const u2 = new URL("/path?a=1", "https://example.com");
console.log(u2.href);

const sp = new URLSearchParams("a=1&b=2&a=3");
console.log(sp.get("a"));
console.log(sp.getAll("a"));
console.log(sp.has("b"));
sp.append("c", "4");
sp.delete("b");
console.log(sp.toString());

const sp2 = new URLSearchParams({ x: "1", y: "2" });
console.log(sp2.toString());

try {
  new URL("not a url");
  console.log("should not reach");
} catch (e) {
  console.log("caught invalid url");
}

const { URL: NamedURL } = require("url");
console.log(new NamedURL("https://x.com/").href);

const http = require("http");

const h = new Headers({ "X-Test": "1" });
h.append("x-test", "2");
console.log(h.get("x-test"));
h.set("content-type", "application/json");
console.log([...h.keys()].join(","));

const req = new Request("http://example.com/foo", { method: "POST", body: "hello", headers: { "X-Custom": "yes" } });
console.log(req.url, req.method, req.headers.get("x-custom"), req.headers.get("content-type"));

const server = http.createServer((request, response) => {
  let body = "";
  request.on("data", (c) => (body += c.toString()));
  request.on("end", () => {
    if (request.url === "/echo") {
      response.writeHead(200, { "Content-Type": "application/json" });
      response.end(JSON.stringify({ method: request.method, body, header: request.headers["x-custom"] }));
    } else if (request.url === "/status") {
      response.writeHead(404, { "Content-Type": "text/plain" });
      response.end("not found");
    } else {
      response.end("ok");
    }
  });
});

server.listen(0, async () => {
  const port = server.address().port;
  const base = `http://localhost:${port}`;

  const res1 = await fetch(base + "/echo", {
    method: "POST",
    body: JSON.stringify({ hello: "world" }),
    headers: { "X-Custom": "abc", "Content-Type": "application/json" },
  });
  console.log("res1 ok:", res1.ok, res1.status);
  const json1 = await res1.json();
  console.log("res1 json:", json1.method, json1.header, json1.body);

  const res2 = await fetch(base + "/status");
  console.log("res2 status:", res2.status, res2.ok);
  const text2 = await res2.text();
  console.log("res2 text:", text2);

  const fd = new FormData();
  fd.append("field1", "value1");
  const res3 = await fetch(base + "/echo", { method: "POST", body: fd });
  const json3 = await res3.json();
  console.log("res3 body has field1:", json3.body.includes("field1"));

  const ac = new AbortController();
  const fetchPromise = fetch(base + "/echo", { signal: ac.signal });
  ac.abort("cancelled by test");
  try {
    await fetchPromise;
    console.log("should not reach");
  } catch (e) {
    console.log("abort worked:", e);
  }

  server.close(() => console.log("done"));
});

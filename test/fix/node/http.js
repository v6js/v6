const http = require("http");

const server = http.createServer((req, res) => {
  let body = "";
  req.on("data", (chunk) => (body += chunk.toString()));
  req.on("end", () => {
    if (req.method === "POST") {
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end("post-body:" + body);
    } else {
      res.setHeader("X-Custom", "yes");
      res.end("hello from server");
    }
  });
});

server.listen(18924, () => {
  console.log("address port matches:", server.address().port === 18924);
  http.get("http://localhost:18924/", (res) => {
    console.log("GET status:", res.statusCode);
    console.log("GET header:", res.headers["x-custom"]);
    let data = "";
    res.on("data", (chunk) => (data += chunk.toString()));
    res.on("end", () => {
      console.log("GET body:", data);

      const req = http.request(
        { hostname: "localhost", port: 18924, path: "/", method: "POST" },
        (res2) => {
          let data2 = "";
          res2.on("data", (chunk) => (data2 += chunk.toString()));
          res2.on("end", () => {
            console.log("POST status:", res2.statusCode);
            console.log("POST body:", data2);
            server.close(() => console.log("server closed"));
          });
        }
      );
      req.end("payload-data");
    });
  });
});

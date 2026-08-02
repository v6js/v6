const https = require("https");
const fs = require("fs");

try {
  https.createServer(() => {});
  console.log("should not reach");
} catch (e) {
  console.log("createServer without key/cert throws as documented");
}

console.log(typeof https.request);
console.log(typeof https.get);

const key = fs.readFileSync("test/fix/node/tls_certs/key.pem");
const cert = fs.readFileSync("test/fix/node/tls_certs/cert.pem");

const server = https.createServer({ key, cert }, (req, res) => {
  res.end("secure-hello");
});

server.listen(18940, () => {
  https.get(
      { hostname: "localhost", port: 18940, path: "/", rejectUnauthorized: false },
      (res) => {
        let data = "";
        res.on("data", (c) => (data += c.toString()));
        res.on("end", () => {
          console.log("https status:", res.statusCode);
          console.log("https body:", data);
          server.close(() => console.log("https server closed"));
        });
      });
});

const tls = require("tls");
const fs = require("fs");

const key = fs.readFileSync("test/fix/node/tls_certs/key.pem");
const cert = fs.readFileSync("test/fix/node/tls_certs/cert.pem");

const server = tls.createServer({ key, cert }, (socket) => {
  socket.on("data", (d) => socket.write("echo:" + d.toString()));
});

server.listen(18941, () => {
  const client = tls.connect(
      { port: 18941, host: "localhost", rejectUnauthorized: false }, () => {
        client.write("hi-tls");
      });
  client.on("data", (d) => {
    console.log("tls received:", d.toString());
    client.destroy();
    server.close(() => console.log("tls server closed"));
  });
  client.on("error", (e) => console.log("tls client error:", e));
});

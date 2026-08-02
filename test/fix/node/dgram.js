const dgram = require("dgram");

const server = dgram.createSocket("udp4");
server.on("message", (msg, rinfo) => {
  console.log("server got:", msg.toString());
  console.log("rinfo port valid:", rinfo.port > 0);
  server.close();
});
server.bind(0, () => {
  const addr = server.address();
  const client = dgram.createSocket("udp4");
  client.send("hello-udp", addr.port, "127.0.0.1", (err) => {
    console.log("send err:", err);
    client.close();
  });
});

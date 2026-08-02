const net = require("net");

const server = net.createServer((socket) => {
  socket.on("data", (data) => {
    socket.write("echo:" + data.toString());
  });
});

server.listen(18923, () => {
  const client = net.connect(18923, "127.0.0.1", () => {
    client.write("hello-server");
  });
  client.on("data", (data) => {
    console.log("client received:", data.toString());
    client.destroy();
    server.close(() => console.log("server closed"));
  });
});

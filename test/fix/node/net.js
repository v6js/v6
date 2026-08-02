const net = require("net");
const os = require("os");
const path = require("path");

function runUnixTest() {
  const sockPath = path.join(os.tmpdir(), "v6_net_fixture.sock");
  const unixServer = net.createServer((socket) => {
    socket.on("data", (data) => {
      socket.write("unix-echo:" + data.toString());
    });
  });

  unixServer.listen(sockPath, () => {
    const unixClient = net.connect(sockPath, () => {
      unixClient.write("hello-unix");
    });
    unixClient.on("data", (data) => {
      console.log("unix client received:", data.toString());
      unixClient.destroy();
      unixServer.close(() => console.log("unix server closed"));
    });
  });
}

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
    client.setNoDelay(true);
    client.setKeepAlive(true);
    console.log("options set ok");
    client.destroy();
    server.close(() => {
      console.log("server closed");
      runUnixTest();
    });
  });
});

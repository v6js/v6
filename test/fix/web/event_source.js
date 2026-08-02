const net = require("net");

const server = net.createServer((sock) => {
  sock.on("data", () => {
    sock.write(
      "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: keep-alive\r\n\r\n"
    );
    sock.write("data: first message\n\n");
    setTimeout(() => {
      sock.write("event: custom\ndata: line1\ndata: line2\nid: 42\n\n");
    }, 50);
    setTimeout(() => {
      sock.write("data: after id\n\n");
    }, 100);
  });
});

server.listen(0, () => {
  const port = server.address().port;
  const es = new EventSource(`http://localhost:${port}/`);
  const received = [];

  es.addEventListener("open", () => console.log("opened"));

  es.onmessage = (e) => {
    received.push("message:" + e.data);
  };

  es.addEventListener("custom", (e) => {
    received.push("custom:" + e.data + ":id=" + e.lastEventId);
  });

  setTimeout(() => {
    console.log(received.join("|"));
    es.close();
    console.log("readyState after close:", es.readyState === EventSource.CLOSED);
    server.close(() => console.log("done"));
  }, 400);
});

const { port1, port2 } = new MessageChannel();

console.log(typeof port1.postMessage, typeof port1.addEventListener);

port2.onmessage = (e) => {
  console.log("port2 got:", e.data, e instanceof MessageEvent, e.type);
};

port1.addEventListener("message", (e) => {
  console.log("port1 got:", e.data);
});

port1.postMessage({ hello: "world", n: 42 });
port2.postMessage("reply");

const me = new MessageEvent("message", { data: "manual" });
console.log("manual event data:", me.data, me.type);

setTimeout(() => {
  console.log("done");
  port1.close();
  port2.close();
}, 20);

const wt = require("worker_threads");

console.log(wt.isMainThread);
console.log(wt.parentPort);
console.log(wt.threadId);

const { port1, port2 } = new wt.MessageChannel();
port2.on("message", (msg) => {
  console.log("port2 got:", msg);
  port1.close();
  port2.close();
});
port1.postMessage("hello-channel");

console.log(typeof wt.Worker);

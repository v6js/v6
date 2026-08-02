console.log("start");

setTimeout(() => console.log("timeout 10"), 10);

const id = setInterval(() => console.log("interval tick"), 20);
setTimeout(() => {
  clearInterval(id);
  console.log("interval cleared");
}, 65);

const immId = setImmediate(() => console.log("should not print"));
clearImmediate(immId);
setImmediate(() => console.log("immediate"));

queueMicrotask(() => console.log("microtask"));

process.on("exit", (code) => console.log("exit event, code=" + code));

console.log("end sync");

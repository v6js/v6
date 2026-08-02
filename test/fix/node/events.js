const EventEmitter = require("events").EventEmitter;

const emitter = new EventEmitter();
emitter.on("greet", (name) => console.log("hello " + name));
emitter.emit("greet", "world");

let onceCount = 0;
emitter.once("ping", () => { onceCount++; console.log("ping " + onceCount); });
emitter.emit("ping");
emitter.emit("ping");

class Listener {
  handle(msg) { console.log("handled: " + msg); }
}
const listener = new Listener();
const boundHandle = (msg) => listener.handle(msg);
emitter.on("msg", boundHandle);
emitter.emit("msg", "hi");
emitter.off("msg", boundHandle);
console.log(emitter.emit("msg", "hi again"));

console.log(emitter.listenerCount("greet"));
console.log(emitter.eventNames());

class Ticker extends EventEmitter {
  tick() {
    this.emit("tick", 1);
  }
}
const t = new Ticker();
t.on("tick", (n) => console.log("tick " + n));
t.tick();

try {
  emitter.emit("error", new Error("boom"));
  console.log("should not reach here");
} catch (e) {
  console.log("caught error event:", e.message);
}

let order = [];
emitter.on("x", () => order.push("normal"));
emitter.prependListener("x", () => order.push("prepended"));
emitter.emit("x");
console.log(order);

let onceOrder = [];
emitter.on("y", () => onceOrder.push("normal"));
emitter.prependOnceListener("y", () => onceOrder.push("prepended-once"));
emitter.emit("y");
emitter.emit("y");
console.log(onceOrder);

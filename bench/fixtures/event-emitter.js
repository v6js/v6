const { EventEmitter } = require("events");

function run() {
  const emitter = new EventEmitter();
  let total = 0;

  emitter.on("tick", (n) => {
    total += n;
  });
  emitter.on("tick", (n) => {
    total += n * 2;
  });
  emitter.on("tock", (n) => {
    total -= n;
  });

  for (let i = 0; i < 50000; i++) {
    emitter.emit("tick", i);
    if (i % 3 === 0) {
      emitter.emit("tock", i);
    }
  }

  return total;
}

console.log(run());

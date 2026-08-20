const fs = require("fs");

const bytes = fs.readFileSync(__dirname + "/primes.wasm");
const stubWasi = {
  fd_write: () => 0,
  proc_exit: () => {},
};

WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stubWasi }).then(
  (result) => {
    const start = Date.now();
    const r = result.instance.exports.run();
    const elapsed = Date.now() - start;
    console.log("result=" + r + " ms=" + elapsed);
  }
);

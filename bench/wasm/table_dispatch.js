const fs = require("fs");

const bytes = fs.readFileSync(__dirname + "/table_dispatch.wasm");
const stubWasi = {
  fd_write: () => 0,
  proc_exit: () => {},
};

WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: stubWasi }).then(
  (result) => {
    console.log(result.instance.exports.run());
  }
);

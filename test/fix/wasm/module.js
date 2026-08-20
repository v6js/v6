const fs = require("fs");
const path = require("path");

const bytes = fs.readFileSync(path.join(__dirname, "add.wasm"));

WebAssembly.compile(bytes).then((module) => {
  return WebAssembly.instantiate(module).then((instance) => {
    console.log(instance.exports.add(4, 9));
  });
});

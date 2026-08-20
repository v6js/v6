const fs = require("fs");
const path = require("path");

const bytes = fs.readFileSync(path.join(__dirname, "add.wasm"));

WebAssembly.instantiate(bytes).then((result) => {
  console.log(result.instance.exports.add(2, 3));
  console.log(result.instance.exports.add(-7, 10));
});

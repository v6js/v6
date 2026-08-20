const fs = require("fs");
const path = require("path");

const bytes = fs.readFileSync(path.join(__dirname, "table.wasm"));

WebAssembly.instantiate(bytes).then((result) => {
  console.log(result.instance.exports.run());
});

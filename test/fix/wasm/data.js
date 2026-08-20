const fs = require("fs");
const path = require("path");

const bytes = fs.readFileSync(path.join(__dirname, "data.wasm"));

WebAssembly.instantiate(bytes).then((result) => {
  console.log(result.instance.exports.run());
});

const fs = require("fs");
const path = require("path");

const bytes = fs.readFileSync(path.join(__dirname, "select.wasm"));

WebAssembly.instantiate(bytes).then((result) => {
  console.log(result.instance.exports.sel(11, 22, 1));
  console.log(result.instance.exports.sel(11, 22, 0));
});

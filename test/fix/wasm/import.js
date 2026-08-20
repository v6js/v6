const fs = require("fs");
const path = require("path");

const bytes = fs.readFileSync(path.join(__dirname, "import.wasm"));

WebAssembly.instantiate(bytes, { env: { dbl: (x) => x * 2 } }).then(
  (result) => {
    console.log(result.instance.exports.dbl(21));
  }
);

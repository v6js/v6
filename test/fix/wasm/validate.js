const fs = require("fs");
const path = require("path");

const good = fs.readFileSync(path.join(__dirname, "add.wasm"));
const bad = new Uint8Array([1, 2, 3, 4]);

console.log(WebAssembly.validate(good));
console.log(WebAssembly.validate(bad));

const fs = require("fs");
const path = require("path");

function run() {
  const dir = path.join(__dirname, "..", "data", "single-dir");
  let total = 0;

  for (let i = 0; i < 200; i++) {
    const entries = fs.readdirSync(dir);
    total += entries.length;
  }

  return total;
}

console.log(run());

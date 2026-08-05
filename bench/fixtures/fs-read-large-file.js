const fs = require("fs");
const path = require("path");

function run() {
  const file = path.join(__dirname, "..", "data", "large-file.txt");
  let total = 0;

  for (let i = 0; i < 3; i++) {
    const content = fs.readFileSync(file, "utf8");
    total += content.length;
  }

  return total;
}

console.log(run());

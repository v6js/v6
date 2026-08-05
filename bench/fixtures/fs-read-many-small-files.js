const fs = require("fs");
const path = require("path");

function run() {
  const dir = path.join(__dirname, "..", "data", "small-files");
  const files = fs.readdirSync(dir).slice(0, 600);
  let total = 0;

  for (let i = 0; i < files.length; i++) {
    const content = fs.readFileSync(path.join(dir, files[i]), "utf8");
    total += content.length;
  }

  return total;
}

console.log(run());

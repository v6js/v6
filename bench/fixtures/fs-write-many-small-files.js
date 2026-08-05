const fs = require("fs");
const path = require("path");

function run() {
  const dir = path.join(__dirname, "..", "data", "write-scratch");
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }

  let total = 0;
  for (let i = 0; i < 300; i++) {
    const file = path.join(dir, "w" + i + ".txt");
    const content = "generated content for file " + i + "\n";
    fs.writeFileSync(file, content);
    total += content.length;
  }

  return total;
}

console.log(run());

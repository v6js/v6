const fs = require("fs");
const path = require("path");

function run() {
  const file = path.join(__dirname, "..", "data", "large-file.txt");
  let total = 0;

  for (let i = 0; i < 5000; i++) {
    const st = fs.statSync(file);
    total += st.size;
  }

  return total;
}

console.log(run());

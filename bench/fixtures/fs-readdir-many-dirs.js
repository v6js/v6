const fs = require("fs");
const path = require("path");

function walk(dir) {
  let count = 0;
  const entries = fs.readdirSync(dir);

  for (let i = 0; i < entries.length; i++) {
    const full = path.join(dir, entries[i]);
    const st = fs.statSync(full);
    if (st.isDirectory()) {
      count += 1 + walk(full);
    } else {
      count += 1;
    }
  }

  return count;
}

function run() {
  const root = path.join(__dirname, "..", "data", "many-dirs");
  let total = 0;

  for (let i = 0; i < 2; i++) {
    total += walk(root);
  }

  return total;
}

console.log(run());

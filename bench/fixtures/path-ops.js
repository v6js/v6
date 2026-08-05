const path = require("path");

function run() {
  let total = 0;
  for (let i = 0; i < 500000; i++) {
    const joined = path.join("a", "b" + (i % 100), "c", "file" + i + ".txt");
    const resolved = path.resolve(joined);
    const parsed = path.parse(joined);
    total += joined.length + resolved.length + parsed.base.length;
  }
  return total;
}

console.log(run());

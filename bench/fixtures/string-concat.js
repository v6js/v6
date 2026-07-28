function buildString(n) {
  let s = "";
  for (let i = 0; i < n; i++) {
    s += "x";
  }
  return s;
}

function run() {
  const result = buildString(2000000);
  return result.length;
}

console.log(run());

function sumTo(n) {
  let total = 0;
  for (let i = 0; i < n; i++) {
    total += i;
  }
  return total;
}

function run() {
  const result = sumTo(20000000);
  return result;
}

console.log(run());

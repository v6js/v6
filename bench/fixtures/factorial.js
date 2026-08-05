function factorial(n) {
  if (n <= 1) return 1;
  return n * factorial(n - 1);
}

function run() {
  let total = 0;
  for (let i = 0; i < 200000; i++) {
    total += factorial(12);
  }
  return total;
}

console.log(run());

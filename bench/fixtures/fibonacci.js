function fib(n) {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
}

function run() {
  const result = fib(32);
  return result;
}

console.log(run());

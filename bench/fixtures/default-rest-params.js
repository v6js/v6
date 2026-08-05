function combine(a, b = 10, c = a + b, ...rest) {
  let sum = a + b + c;
  for (let i = 0; i < rest.length; i++) {
    sum += rest[i];
  }
  return sum;
}

function run() {
  let total = 0;
  for (let i = 0; i < 100000; i++) {
    total += combine(i);
    total += combine(i, i + 1);
    total += combine(i, i + 1, i + 2, 1, 2, 3);
  }
  return total;
}

console.log(run());

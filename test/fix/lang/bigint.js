let a = 10n;
let b = 3n;
console.log(typeof a);
console.log(a + b);
console.log(a - b);
console.log(a * b);
console.log(a / b);
console.log(a % b);
console.log(a ** b);

console.log(a > b);
console.log(a < b);
console.log(a >= 10n);
console.log(a <= b);
console.log(a === 10n);
console.log(a === b);
console.log(a == 10);
console.log(a === 10);

let big = 123456789012345678901234567890n;
console.log(big);
console.log(big + 1n);
console.log(-a);

console.log(a & b);
console.log(a | b);
console.log(a ^ b);
console.log(~a);
console.log(a << 2n);
console.log(a >> 1n);

console.log(BigInt(42));
console.log(BigInt("999"));
console.log(typeof BigInt(5));

console.log(!!0n);
console.log(!!5n);

console.log("value: " + a);
console.log(`interpolated: ${a}`);

function factorial(n) {
  let result = 1n;
  for (let i = 2n; i <= n; i = i + 1n) {
    result = result * i;
  }
  return result;
}
console.log(factorial(20n));

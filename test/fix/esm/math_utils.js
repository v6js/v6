export function add(a, b) {
  return a + b;
}

export function multiply(a, b) {
  return a * b;
}

export const PI_APPROX = 3.14159;

class Calculator {
  constructor(start) {
    this.value = start;
  }
  add(n) {
    this.value = this.value + n;
    return this;
  }
}
export { Calculator };

export default function square(x) {
  return x * x;
}

console.log("math_utils module evaluated");

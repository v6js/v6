function add(a, b) {
  return a + b;
}

function square(x) {
  return x * x;
}

function hypotSquared(a, b) {
  return square(a) + square(b);
}

console.log(add(2, 3));
console.log(square(5));
console.log(hypotSquared(3, 4));

function greet(name = "world") {
  return "hi " + name;
}
console.log(greet());
console.log(greet("Ada"));

function accumulate(a, b = a + 1, c = a + b) {
  return a + b + c;
}
console.log(accumulate(1));
console.log(accumulate(1, 5));
console.log(accumulate(1, 5, 10));

const scale = (x = 10, y = x * 2) => x + y;
console.log(scale());
console.log(scale(3));
console.log(scale(3, 100));

function withUndefined(a = 1, b = 2) {
  return a + "," + b;
}
console.log(withUndefined(undefined, 5));

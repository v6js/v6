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

function describePerson({ name, age }) {
  return name + " is " + age;
}
console.log(describePerson({ name: "Ada", age: 36 }));

function withDefaultPattern({ a, b = 10 } = {}) {
  return a + "," + b;
}
console.log(withDefaultPattern({ a: 1 }));
console.log(withDefaultPattern());

function firstTwo([x, y]) {
  return x + y;
}
console.log(firstTwo([3, 4]));

function withRestPattern([head, ...tail]) {
  return head + ":" + tail.length;
}
console.log(withRestPattern([1, 2, 3, 4]));

function mixedParams(a, { b, c }, [d, e]) {
  return a + b + c + d + e;
}
console.log(mixedParams(1, { b: 2, c: 3 }, [4, 5]));

const arrowDestructure = ({ x, y }) => x * y;
console.log(arrowDestructure({ x: 6, y: 7 }));

function renamedParam({ x: px, y: py = 100 }) {
  return px + py;
}
console.log(renamedParam({ x: 1 }));

function nestedParam({ outer: { inner } }) {
  return inner;
}
console.log(nestedParam({ outer: { inner: 42 } }));

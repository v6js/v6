function add(first, second) {
  let sum = first + second;
  return sum;
}
console.log(add(2, 3));

function withClosure(base) {
  let multiplier = 10;
  function inner(extra) {
    return (base + extra) * multiplier;
  }
  return inner(1);
}
console.log(withClosure(5));

function shadowed(x) {
  let y = x + 1;
  if (y > 0) {
    let y = 100;
    console.log(y);
  }
  return y;
}
console.log(shadowed(1));

function destructure({ a, b }) {
  return a + b;
}
console.log(destructure({ a: 1, b: 2 }));

function shorthandObj(name, value) {
  return { name, value };
}
console.log(JSON.stringify(shorthandObj("k", "v")));

function tryCatchTest() {
  try {
    throw new Error("x");
  } catch (err) {
    return err.message;
  }
}
console.log(tryCatchTest());

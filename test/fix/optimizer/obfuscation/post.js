function add(a, b) {
  let c = a + b;
  return c;
}
console.log(add(2, 3));
function withClosure(a) {
  let b = 10;
  function inner(c) {
    return (a + c) * b;
  }
  return inner(1);
}
console.log(withClosure(5));
function shadowed(a) {
  let y = a + 1;
  if (y > 0) {
    let y = 100;
    console.log(y);
  }
  return y;
}
console.log(shadowed(1));
function destructure({a: c, b: d}) {
  return c + d;
}
console.log(destructure({a: 1, b: 2}));
function shorthandObj(a, b) {
  return {name: a, value: b};
}
console.log(JSON.stringify(shorthandObj("k", "v")));
function tryCatchTest() {
  try {
    throw new Error("x");
  } catch (a) {
    return a.message;
  }
}
console.log(tryCatchTest());

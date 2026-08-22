function double(x) {
  return x * 2;
}
console.log(21 * 2);
function add3(a, b, c) {
  return a + b + c;
}
console.log(1 + 2 + 3);
function usedTwice(x) {
  return x + 1;
}
console.log(usedTwice(1) + usedTwice(2));
function calledWithSideEffect(x) {
  return x * 2;
}
let counter = 0;
function next() {
  counter++;
  return counter;
}
console.log(calledWithSideEffect(next()));
function referencesOuter(x) {
  return x + outerValue;
}
let outerValue = 100;
console.log(referencesOuter(5));
function multiStatement(x) {
  const y = x + 1;
  return y * 2;
}
console.log(multiStatement(5));
function wrongArity(a, b) {
  return a + b;
}
console.log(wrongArity(1, 2, 3));
function usedAsValue(x) {
  return x + 1;
}
const ref = usedAsValue;
console.log(ref(10));
function repeatedParam(x) {
  return x * x + x;
}
console.log(4 * 4 + 4);
async function asyncFn(x) {
  return x + 1;
}
console.log(typeof asyncFn(1));

function basic(a, b) {
  const x = a * b + 1;
  const y = x;
  return x + y;
}
console.log(basic(2, 3));
function threeInARow(a, b) {
  const x = a + b;
  const y = x;
  const z = x;
  return x + y + z;
}
console.log(threeInARow(4, 5));
function mutatedBetween(a, b) {
  let a2 = a;
  const x = a2 + b;
  a2 = a2 + 1;
  const y = a2 + b;
  return x + y;
}
console.log(mutatedBetween(1, 2));
function differentExpr(a, b) {
  const x = a + b;
  const y = a - b;
  return x + y;
}
console.log(differentExpr(10, 3));
function letNotConst(a, b) {
  let x = a * b;
  let y = a * b;
  return x + y;
}
console.log(letNotConst(3, 4));
function memberNotCsed(obj) {
  const x = obj.value + 1;
  const y = obj.value + 1;
  return x + y;
}
console.log(memberNotCsed({value: 5}));
function callNotCsed() {
  let calls = 0;
  function f() {
    calls++;
    return 2;
  }
  const x = f() + 1;
  const y = f() + 1;
  return x + y + calls;
}
console.log(callNotCsed());
function acrossStatements(a, b) {
  const x = a * b;
  console.log("between");
  const y = x;
  return x + y;
}
console.log(acrossStatements(2, 5));

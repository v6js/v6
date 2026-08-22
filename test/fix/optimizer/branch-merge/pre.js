function f(cond) {
  if (cond) return 5 + 1;
  else return 5 + 1;
}
console.log(f(true));
console.log(f(false));

let calls = 0;
function sideEffectCond() {
  calls++;
  return true;
}
function g() {
  if (sideEffectCond()) return 42;
  else return 42;
}
console.log(g());
console.log(calls);

function h(cond, obj) {
  if (cond) return obj.value;
  else return obj.value;
}
console.log(h(true, { value: 9 }));

function diffBranches(cond) {
  if (cond) return 1;
  else return 2;
}
console.log(diffBranches(true));
console.log(diffBranches(false));

function exprStmtBranches(cond) {
  let x = 0;
  if (cond) x = 1;
  else x = 1;
  return x;
}
console.log(exprStmtBranches(true));

function bareReturns(cond) {
  if (cond) return;
  else return;
  return 99;
}
console.log(bareReturns(true));

function noElse(cond) {
  if (cond) return 1;
  return 2;
}
console.log(noElse(true));
console.log(noElse(false));

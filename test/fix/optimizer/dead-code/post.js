function unreachableAfterReturn() {
  console.log("before");
  return 1;
}
function deadIfFalse(x) {
  console.log("after");
  return x;
}
function keepIfTrue(x) {
  console.log("kept branch");
  return x;
}
function deadWhile() {
  console.log("reached");
}
console.log(unreachableAfterReturn());
console.log(deadIfFalse(5));
console.log(keepIfTrue(6));
deadWhile();

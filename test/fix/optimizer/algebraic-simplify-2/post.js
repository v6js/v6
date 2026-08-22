let calls = 0;
function sideEffect() {
  calls++;
  return 5;
}
console.log(sideEffect() ** 0);
console.log(calls);
console.log(2);
console.log(NaN);
console.log(Infinity);
console.log(-0);
let x = 5;
console.log(x);
console.log(x);
console.log(x);
let y = null;
console.log(y);

console.log((1, 2, 3));
console.log((sideEffect(), 5));
function sideEffect() {
  console.log("effect");
  return 99;
}
let x = (1, "hi");
console.log(x);
console.log(true, false);
console.log(true.toString());
console.log(false.toString());
let arr = [true, false, true];
console.log(JSON.stringify(arr));
function usesBool(b) {
  return b === true;
}
console.log(usesBool(true));

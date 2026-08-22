let calls = 0;
function sideEffect() { calls++; return 5; }
console.log(sideEffect() ** 0);
console.log(calls);
console.log((2) ** 1);
console.log(NaN ** 1);
console.log(Infinity ** 1);
console.log((-0) ** 1);

let x = 5;
console.log(x || x);
console.log(x && x);
console.log(x ?? x);
let y = null;
console.log(y ?? y);

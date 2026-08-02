const { performance } = require("perf_hooks");

console.log(typeof performance.now());
console.log(performance.now() >= 0);
const a = performance.now();
const b = performance.now();
console.log(b >= a);
console.log(typeof performance.timeOrigin);

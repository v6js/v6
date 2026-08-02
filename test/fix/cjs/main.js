const greeter = require("./greet.js");
console.log(greeter.greet("World"));
console.log(greeter.farewell("World"));

const math = require("./math.js");
console.log(math.add(2, 3));
console.log(math.multiply(4, 5));

const increment = require("./increment.js");
console.log(increment(41));

const shout = require("str-lib").shout;
console.log(shout("quiet"));

function useRequireInsideFunction() {
  const inner = require("./increment.js");
  return inner(9);
}
console.log(useRequireInsideFunction());

if (true) {
  const conditional = require("./math.js");
  console.log(conditional.add(10, 20));
}

const circA = require("./circ_a.js");
const circB = require("./circ_b.js");
console.log(circA.getFromA());
console.log(circB.getFromB());

console.log("main done");

import square, { add, multiply, PI_APPROX, Calculator } from "./math_utils.js";
import * as mathUtils from "./math_utils";
import "./side_effect.js";
import { greet } from "greeter-lib";
import { label } from "./sub";
import { getFromA } from "./circ_a.js";

console.log("main starting");
console.log(add(2, 3));
console.log(multiply(4, 5));
console.log(PI_APPROX);
console.log(square(6));

let calc = new Calculator(10);
calc.add(5).add(3);
console.log(calc.value);

console.log(mathUtils.add(1, 1));
console.log(mathUtils.default(7));

console.log(greet("World"));
console.log(label);
console.log(getFromA());

console.log("main done");

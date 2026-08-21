import * as a from "./a.js";

export function fromB() {
  return "b";
}

export function callA() {
  return "b calls " + a.fromA();
}

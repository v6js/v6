import * as b from "./b.js";

export function fromA() {
  return "a";
}

export function callB() {
  return "a calls " + b.fromB();
}

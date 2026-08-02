import { bValue } from "./circ_b.js";
export const aValue = "A";
export function getFromA() {
  return "A sees b=" + bValue;
}
console.log("circ_a loaded, bValue seen at load time:", bValue);

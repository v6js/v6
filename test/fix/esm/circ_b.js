import { aValue } from "./circ_a.js";
export const bValue = "B";
export function getFromB() {
  return "B sees a=" + aValue;
}
console.log("circ_b loaded, aValue seen at load time:", aValue);

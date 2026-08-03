import { ArrayList } from "java:java.util";
import Integer from "java:java.lang.Integer";

const list = new ArrayList();
list.add("only one");

try {
  list.get(99);
  console.log("should not reach here");
} catch (e) {
  const msg = `${e}`;
  console.log("caught:", typeof e, msg.includes("Index"));
}

try {
  Integer.parseInt("not a number");
  console.log("should not reach here");
} catch (e) {
  console.log("caught:", `${e}`.includes("NumberFormatException"));
}

try {
  console.log("valid access ok:", list.get(0));
} catch (e) {
  console.log("unexpected error:", e);
}

console.log("done");

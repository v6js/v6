import Greeter from "java:Greeter";

const g = new Greeter("v6");
console.log(g.greet());

Greeter.greeting = "Hi";
const g2 = new Greeter("world");
console.log(g2.greet());
console.log(g.greet());

console.log(Greeter.add(3, 4));

try {
  Greeter.throwIfNegative(-5);
} catch (e) {
  console.log("caught:", `${e}`.includes("negative"));
}

Greeter.throwIfNegative(5);
console.log("no throw for positive");

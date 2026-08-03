import ArrayList from "java:java.util.ArrayList";
import Math2 from "java:java.lang.Math";

const list = new ArrayList();
list.add("hello");
list.add("world");
console.log(list.size());
console.log(list.get(0), list.get(1));
console.log(list.toString());

console.log(Math2.PI);
console.log(Math2.abs(-42));
console.log(Math2.max(3, 7));
console.log(Math2.sqrt(16));

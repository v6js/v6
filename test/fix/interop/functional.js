import { ArrayList, Collections } from "java:java.util";
import Thread from "java:java.lang.Thread";

const list = new ArrayList();
list.add("banana");
list.add("apple");
list.add("cherry");

Collections.sort(list, (a, b) => a.length - b.length);
console.log(list.toString());

Collections.sort(list, (a, b) => {
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
});
console.log(list.toString());

const t = new Thread(() => {
  console.log("running in a real java.lang.Thread");
});
t.start();
t.join();

let calledCount = 0;
const runner = { run: () => { calledCount++; } };
const t2 = new Thread(runner);
t2.start();
t2.join();
console.log("calledCount:", calledCount);

console.log("all done");

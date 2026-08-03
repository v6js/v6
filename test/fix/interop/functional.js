import { ArrayList, Collections } from "java:java.util";
import Thread from "java:java.lang.Thread";
import Optional from "java:java.util.Optional";

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

list.replaceAll((s) => s.toUpperCase());
console.log(list.toString());

list.removeIf((s) => s.startsWith("B"));
console.log(list.toString());

const empty = Optional.empty();
console.log(empty.orElseGet(() => "default value"));

const present = Optional.of("real value");
console.log(present.orElseGet(() => "unused"));

const kept = Optional.of(42).filter((n) => n > 10);
console.log(kept.isPresent(), kept.get());

const dropped = Optional.of(5).filter((n) => n > 10);
console.log(dropped.isPresent());

const mapped = Optional.of(5).map((n) => n * 2);
console.log(mapped.get());

console.log("all done");

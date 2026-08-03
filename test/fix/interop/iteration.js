import { ArrayList, HashMap } from "java:java.util";

const list = new ArrayList();
list.add("a");
list.add("b");
list.add("c");

const it = list.iterator();
const collected = [];
while (it.hasNext()) {
  collected.push(it.next());
}
console.log(collected.join(","));

const map = new HashMap();
map.put("x", 1);
map.put("y", 2);

const entries = map.entrySet();
const eit = entries.iterator();
const pairs = [];
while (eit.hasNext()) {
  const entry = eit.next();
  pairs.push(entry.getKey() + "=" + entry.getValue());
}
pairs.sort();
console.log(pairs.join(","));

const keys = map.keySet();
const kit = keys.iterator();
const keyList = [];
while (kit.hasNext()) keyList.push(kit.next());
keyList.sort();
console.log(keyList.join(","));

console.log("--- for-of directly over a Java List: not supported, does not iterate ---");
let count = 0;
for (const x of list) {
  count++;
}
console.log("for-of count:", count);

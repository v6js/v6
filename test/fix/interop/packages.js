import { HashMap, Random, Arrays } from "java:java.util";
import * as jl from "java:java.lang";

const map = new HashMap();
map.put("x", 1);
map.put("y", 2);
console.log(map.get("x"), map.get("y"), map.size());

const r = new Random(42);
console.log(typeof r.nextInt(100));

console.log(jl.Integer.parseInt("123"));
console.log(jl.String.valueOf(true));

const list = Arrays.asList("a", "b", "c");
console.log(list.toString(), list.size());

import { Map } from "java:java.util";
console.log(typeof Map.Entry);

import StringJoiner from "java:java.util.StringJoiner";
const sj = new StringJoiner(", ", "[", "]");
sj.add("x").add("y").add("z");
console.log(sj.toString());

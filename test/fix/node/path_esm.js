import path from "path";
import pathNs from "node:path";
console.log(path === pathNs);
console.log(path.join("x", "y", "z"));
console.log(path.dirname("/x/y/z.txt"));

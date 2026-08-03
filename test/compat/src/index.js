import { jsonFiles, allFiles } from "./fast-glob.js";

console.log("index.js sees jsonFiles:", jsonFiles);
console.log("index.js sees allFiles count:", allFiles.length);

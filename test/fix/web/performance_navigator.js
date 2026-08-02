console.log(typeof performance.now, typeof performance.mark, typeof performance.measure);
console.log(typeof navigator.hardwareConcurrency, navigator.hardwareConcurrency > 0);
console.log(navigator.userAgent, navigator.language);

performance.mark("start");
let sum = 0;
for (let i = 0; i < 100000; i++) sum += i;
performance.mark("end");
const m = performance.measure("work", "start", "end");
console.log("measure duration >= 0:", m.duration >= 0, m.name, m.entryType);

console.log("entries by type mark:", performance.getEntriesByType("mark").length);
console.log("entries by name start:", performance.getEntriesByName("start").length);
console.log("all entries:", performance.getEntries().length);

performance.clearMarks();
console.log("after clearMarks, marks left:", performance.getEntriesByType("mark").length);
console.log("measures still there:", performance.getEntriesByType("measure").length);

performance.clearMeasures();
console.log("after clearMeasures:", performance.getEntries().length);

console.log("perf_hooks same object:", require("perf_hooks").performance === performance);

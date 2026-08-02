const v8 = require("v8");

const stats = v8.getHeapStatistics();
console.log(stats.heap_size_limit > 0, stats.total_heap_size > 0);
console.log(v8.getHeapSpaceStatistics().length > 0);

const buf = v8.serialize({ a: 1, b: [1, 2, 3] });
console.log(Buffer.isBuffer(buf));
console.log(JSON.stringify(v8.deserialize(buf)));

try {
  v8.writeHeapSnapshot();
  console.log("should not reach");
} catch (e) {
  console.log("writeHeapSnapshot correctly unsupported");
}

console.log(typeof process.argv);
console.log(Array.isArray(process.argv));
console.log(typeof process.env);
console.log(process.platform);
console.log(typeof process.version);
console.log(typeof process.versions);
console.log(process.cwd().length > 0);
process.stdout.write("no-newline-a");
process.stdout.write("no-newline-b\n");
process.stderr.write("err-line\n");

console.log("before tick");
process.nextTick(() => console.log("tick 1"));
process.nextTick(() => console.log("tick 2"));
console.log("after tick");

console.log(typeof process.hrtime());
console.log(process.argv[0]);
console.log(process.argv.slice(2));

process.on("exit", (code) => console.log("exit event, code=" + code));

let uncaughtSeen = false;
process.on("uncaughtException", (err) => {
  uncaughtSeen = true;
  console.log("uncaught handled:", err.message || err);
});
setTimeout(() => { throw new Error("from timer"); }, 5);
setTimeout(() => console.log("uncaughtSeen:", uncaughtSeen), 20);

const mem = process.memoryUsage();
console.log(typeof mem.rss, typeof mem.heapTotal, typeof mem.heapUsed, typeof mem.external,
            typeof mem.arrayBuffers);
console.log(mem.rss > 0, mem.heapTotal > 0);
console.log(typeof process.memoryUsage.rss());

const cpu1 = process.cpuUsage();
console.log(typeof cpu1.user, typeof cpu1.system);
const cpu2 = process.cpuUsage(cpu1);
console.log(cpu2.user >= 0, cpu2.system >= 0);

console.log(typeof process.kill);
process.on("SIGINT", () => {});
console.log("signal handler registered ok");

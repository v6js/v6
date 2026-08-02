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

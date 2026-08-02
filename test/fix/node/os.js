const os = require("os");

console.log(os.platform());
console.log(os.type());
console.log(os.arch());
console.log(typeof os.release());
console.log(os.homedir().length > 0);
console.log(os.tmpdir().length > 0);
console.log(os.EOL === "\n" || os.EOL === "\r\n");
console.log(Array.isArray(os.cpus()));
console.log(os.cpus().length > 0);
console.log(typeof os.totalmem());
console.log(typeof os.freemem());
console.log(typeof os.hostname());
console.log(os.endianness() === "LE" || os.endianness() === "BE");

const fg = require("fast-glob");

const jsonFiles = fg.sync(["*.json"], { cwd: __dirname, absolute: false });
console.log("sync glob (*.json):", jsonFiles);

const allFiles = fg.sync(["**/*"], { cwd: __dirname, onlyFiles: true });
console.log("sync glob (**/*) count:", allFiles.length);

fg(["*.json"], { cwd: __dirname }).then((results) => {
  console.log("async glob result:", results);
});

const stream = fg.stream(["*.json"], { cwd: __dirname });
stream.on("data", (entry) => console.log("stream entry:", entry));
stream.on("end", () => console.log("stream done"));

module.exports = { jsonFiles, allFiles };

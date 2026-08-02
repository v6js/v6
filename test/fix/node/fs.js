const fs = require("fs");
const path = require("path");

const dir = "test/fix/node/tmp_fs";
if (fs.existsSync(dir)) fs.rmSync(dir, { recursive: true, force: true });
fs.mkdirSync(dir, { recursive: true });

const file = path.join(dir, "a.txt");
fs.writeFileSync(file, "hello");
console.log(fs.readFileSync(file, "utf8"));
console.log(fs.readFileSync(file).toString());

fs.appendFileSync(file, " world");
console.log(fs.readFileSync(file, "utf8"));

console.log(fs.existsSync(file));
console.log(fs.existsSync(path.join(dir, "missing.txt")));

fs.mkdirSync(path.join(dir, "sub"));
fs.writeFileSync(path.join(dir, "sub", "b.txt"), "b");
fs.writeFileSync(path.join(dir, "c.txt"), "c");
console.log(fs.readdirSync(dir));

const st = fs.statSync(file);
console.log(st.isFile(), st.isDirectory(), st.size);

const dirSt = fs.statSync(dir);
console.log(dirSt.isFile(), dirSt.isDirectory());

fs.renameSync(file, path.join(dir, "renamed.txt"));
console.log(fs.existsSync(file));
console.log(fs.readFileSync(path.join(dir, "renamed.txt"), "utf8"));

fs.copyFileSync(path.join(dir, "renamed.txt"), path.join(dir, "copy.txt"));
console.log(fs.readFileSync(path.join(dir, "copy.txt"), "utf8"));

fs.unlinkSync(path.join(dir, "copy.txt"));
console.log(fs.existsSync(path.join(dir, "copy.txt")));

try {
  fs.readFileSync(path.join(dir, "does_not_exist.txt"));
} catch (e) {
  console.log("caught: " + (e !== undefined));
}

console.log("sync section done");

fs.readFile(path.join(dir, "renamed.txt"), "utf8", (err, data) => {
  console.log("async read:", err, data);

  fs.writeFile(path.join(dir, "async.txt"), "async-write", (err2) => {
    console.log("async write err:", err2);
    fs.readFile(path.join(dir, "async.txt"), "utf8", (err3, data3) => {
      console.log("async read2:", err3, data3);
      fs.rmSync(dir, { recursive: true, force: true });
      console.log("cleanup done");
    });
  });
});

fs.readFile(path.join(dir, "nope.txt"), "utf8", (err) => {
  console.log("async error present:", err !== null && err !== undefined);
});

console.log("end of sync code");

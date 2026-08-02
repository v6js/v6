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

console.log(fs.constants.F_OK, fs.constants.O_CREAT);

const gapsDir = "test/fix/node/tmp_fs_gaps";
if (fs.existsSync(gapsDir)) fs.rmSync(gapsDir, { recursive: true, force: true });
fs.mkdirSync(gapsDir, { recursive: true });
const gapsFile = path.join(gapsDir, "a.txt");
fs.writeFileSync(gapsFile, "hello");

fs.promises.readFile(gapsFile, "utf8").then((d) => console.log("promises.readFile:", d));
fs.promises.stat(gapsFile).then((st) => console.log("promises.stat size:", st.size));

let watchSeen = false;
const watcher = fs.watch(gapsDir, (evt, name) => {
  watchSeen = true;
});
setTimeout(() => fs.writeFileSync(gapsFile, "changed"), 100);
setTimeout(() => {
  watcher.close();
  console.log("watch saw a change:", watchSeen);
  fs.rmSync(gapsDir, { recursive: true, force: true });
}, 350);

const p2Dir = "test/fix/node/tmp_fs_p2";
if (fs.existsSync(p2Dir)) fs.rmSync(p2Dir, { recursive: true, force: true });
fs.mkdirSync(p2Dir, { recursive: true });
const p2File = path.join(p2Dir, "a.txt");
fs.writeFileSync(p2File, "hello");

const st2 = fs.statSync(p2File);
console.log(st2.mtime instanceof Date, st2.mtime.getTime() === st2.mtimeMs);

fs.writeFileSync(path.join(p2Dir, "b.txt"), "b");
fs.mkdirSync(path.join(p2Dir, "sub"));
const dirents = fs.readdirSync(p2Dir, { withFileTypes: true });
console.log(dirents.map((e) => e.name + ":" + (e.isDirectory() ? "dir" : "file")).sort());

const dh = fs.opendirSync(p2Dir);
let dnames = [];
let dent;
while ((dent = dh.readSync()) !== null) dnames.push(dent.name);
dh.closeSync();
console.log(dnames.sort());

fs.accessSync(p2File, fs.constants.R_OK);
console.log("access ok");
try {
  fs.accessSync(path.join(p2Dir, "nope.txt"), fs.constants.R_OK);
  console.log("should not reach");
} catch (e) {
  console.log("access caught");
}

fs.chmodSync(p2File, 0o644);
console.log("chmod ran");

let p2ReadData = "";
const p2rs = fs.createReadStream(p2File, "utf8");
p2rs.on("data", (chunk) => (p2ReadData += chunk));
p2rs.on("end", () => {
  console.log("stream read:", p2ReadData);
  const p2ws = fs.createWriteStream(path.join(p2Dir, "written.txt"));
  p2ws.on("finish", () => {
    console.log("stream write done:", fs.readFileSync(path.join(p2Dir, "written.txt"), "utf8"));
    fs.rmSync(p2Dir, { recursive: true, force: true });
  });
  p2ws.write("streamed-");
  p2ws.end("content");
});

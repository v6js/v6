const cp = require("child_process");

const out = cp.execSync("echo hello-sync").toString().trim();
console.log(out);

cp.exec("echo hello-exec", (err, stdout, stderr) => {
  console.log("exec err:", err);
  console.log("exec stdout:", stdout.trim());
});

const result = cp.spawnSync(process.platform === "win32" ? "cmd" : "echo",
  process.platform === "win32" ? ["/c", "echo hello-spawnsync"] : ["hello-spawnsync"]);
console.log(result.status);
console.log(result.stdout.toString().trim());

const child = cp.spawn(process.platform === "win32" ? "cmd" : "echo",
  process.platform === "win32" ? ["/c", "echo hello-spawn"] : ["hello-spawn"]);
let chunks = [];
child.stdout.on("data", (d) => chunks.push(d.toString()));
child.on("close", (code) => {
  console.log("spawn close code:", code);
  console.log("spawn output:", chunks.join("").trim());
});

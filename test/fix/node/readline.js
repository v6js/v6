const readline = require("readline");

const rl = readline.createInterface({ input: process.stdin, output: process.stdout });

let lines = [];
rl.on("line", (line) => {
  lines.push(line);
  if (lines.length === 2) {
    console.log("collected:", lines);
    rl.question("name? ", (answer) => {
      console.log("answer:", answer);
      rl.close();
    });
  }
});

rl.on("close", () => console.log("rl closed"));

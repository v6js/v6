function run() {
  let total = 0;

  for (let i = 0; i < 30000; i++) {
    const s = "payload-" + i + "-data";
    const buf = Buffer.from(s, "utf8");
    const b64 = buf.toString("base64");
    const hex = buf.toString("hex");
    const roundtrip = Buffer.from(b64, "base64").toString("utf8");
    total += b64.length + hex.length + roundtrip.length;
  }

  return total;
}

console.log(run());

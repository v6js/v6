function run() {
  let total = 0;
  for (let i = 0; i < 50000; i++) {
    const s = "  value-" + i + "-end  ";
    const trimmed = s.trim();
    const parts = trimmed.split("-");
    const joined = parts.join("_");
    const padded = joined.padStart(20, "0");
    const repeated = parts[0].repeat(2);
    total += padded.length + repeated.length + parts.length;
  }
  return total;
}

console.log(run());

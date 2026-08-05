function run() {
  let total = 0;
  const base = new Date(2020, 0, 1);

  for (let i = 0; i < 30000; i++) {
    const d = new Date(base.getTime() + i * 86400000);
    total += d.getFullYear() + d.getMonth() + d.getDate() + d.getDay();
    total += d.toISOString().length;
  }
  return total;
}

console.log(run());

function buildData(n) {
  const arr = [];
  for (let i = 0; i < n; i++) {
    arr.push({
      id: i,
      name: "item-" + i,
      active: i % 2 === 0,
      tags: ["a", "b", "c"],
      nested: { x: i, y: i * 2, z: i * 3 },
    });
  }
  return arr;
}

function run() {
  const data = buildData(2000);
  let total = 0;

  for (let i = 0; i < 5; i++) {
    const text = JSON.stringify(data);
    const parsed = JSON.parse(text);
    total += parsed.length;
  }

  return total;
}

console.log(run());

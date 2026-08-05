function step(n) {
  return Promise.resolve(n).then((v) => v + 1);
}

async function chain(n) {
  let v = 0;
  for (let i = 0; i < n; i++) {
    v = await step(v);
  }
  return v;
}

async function run() {
  let total = 0;
  for (let i = 0; i < 100; i++) {
    total += await chain(10);
  }
  return total;
}

run().then((result) => console.log(result));

function makeBatch(base, n) {
  const promises = [];
  for (let i = 0; i < n; i++) {
    promises.push(Promise.resolve(base + i));
  }
  return Promise.all(promises);
}

async function run() {
  let total = 0;
  for (let i = 0; i < 200; i++) {
    const results = await makeBatch(i, 10);
    for (let j = 0; j < results.length; j++) {
      total += results[j];
    }
  }
  return total;
}

run().then((result) => console.log(result));

console.log("start");

async function simple() {
  return 42;
}
simple().then((v) => console.log("simple:", v));

function delayed(value) {
  return new Promise((resolve) => {
    resolve(value);
  });
}

async function sequence() {
  console.log("sequence: before");
  const a = await delayed("a");
  const b = await delayed("b");
  return a + b;
}
sequence().then((v) => console.log("sequence result:", v));

console.log("end of sync code");

async function throwsValue() {
  throw "boom";
}
throwsValue().catch((e) => console.log("caught:", e));

async function awaitsRejection() {
  try {
    await Promise.reject("rejected-value");
  } catch (e) {
    return "recovered:" + e;
  }
}
awaitsRejection().then((v) => console.log(v));

Promise.all([Promise.resolve(1), delayed(2), Promise.resolve(3)]).then((results) =>
  console.log("all:", results)
);

Promise.race([delayed("slow"), Promise.resolve("fast")]).then((v) =>
  console.log("race:", v)
);

Promise.allSettled([Promise.resolve("ok"), Promise.reject("bad")]).then(
  (results) => {
    console.log(results[0].status, results[0].value);
    console.log(results[1].status, results[1].reason);
  }
);

const arrow = async (x) => x * 2;
arrow(21).then((v) => console.log("arrow:", v));

async function chained() {
  const p = Promise.resolve(1)
    .then((v) => v + 1)
    .then((v) => v + 1);
  return await p;
}
chained().then((v) => console.log("chained:", v));

class Fetcher {
  constructor(value) {
    this.value = value;
  }
  async load() {
    return this.value * 2;
  }
  static async create(value) {
    return new Fetcher(value);
  }
}
new Fetcher(21).load().then((v) => console.log("method:", v));
Fetcher.create(10).then((instance) => console.log("static method:", instance.value));

const computer = {
  base: 5,
  async compute() {
    return this.base + 1;
  },
};
computer.compute().then((v) => console.log("object method:", v));

async function* ticker() {
  yield 1;
  yield 2;
  await Promise.resolve("waited");
  yield 3;
}
async function drainTicker() {
  let g = ticker();
  let out = [];
  let r;
  while (!(r = await g.next()).done) {
    out.push(r.value);
  }
  console.log("ticker:", out.join(","));
}
drainTicker().then(() => console.log("ticker drained"));

async function* delayedRange(n) {
  for (let i = 0; i < n; i++) {
    await Promise.resolve(i);
    yield i * 10;
  }
}
async function useForAwait() {
  let collected = [];
  for await (const v of delayedRange(4)) {
    collected.push(v);
  }
  console.log("for-await:", collected.join(","));
}
useForAwait().then(() => console.log("for-await done"));

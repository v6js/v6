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

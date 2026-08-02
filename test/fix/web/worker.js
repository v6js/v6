console.log("main thread self:", typeof self);

const w = new Worker("test/fix/web/worker_child.js");
console.log(typeof w.postMessage, typeof w.addEventListener, typeof w.terminate);

w.onmessage = (e) => {
  console.log("main got:", JSON.stringify(e.data), e instanceof MessageEvent);
  w.terminate();
};

w.postMessage({ n: 21 });

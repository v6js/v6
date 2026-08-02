const ac = new AbortController();
console.log(ac.signal.aborted);
let abortEventFired = false;
ac.signal.addEventListener("abort", () => { abortEventFired = true; });
ac.abort("custom reason");
console.log(ac.signal.aborted, ac.signal.reason, abortEventFired);

try {
  ac.signal.throwIfAborted();
  console.log("should not reach");
} catch (e) {
  console.log("threw:", e);
}

const s2 = AbortSignal.abort("preemptive");
console.log(s2.aborted, s2.reason);

let timeoutFired = false;
const s3 = AbortSignal.timeout(10);
s3.onabort = () => { timeoutFired = true; };
setTimeout(() => console.log("timeout signal aborted:", s3.aborted, timeoutFired), 30);

const ac1 = new AbortController();
const ac2 = new AbortController();
const combined = AbortSignal.any([ac1.signal, ac2.signal]);
console.log("combined aborted before:", combined.aborted);
ac2.abort("second");
console.log("combined aborted after:", combined.aborted, combined.reason);

try {
  new DOMException("bad thing", "NotFoundError");
  console.log("DOMException constructible");
} catch (e) {
  console.log("should not reach");
}
const de = new DOMException("bad thing", "NotFoundError");
console.log(de.name, de.message, de instanceof Error);

try {
  new AbortSignal();
  console.log("should not reach");
} catch (e) {
  console.log("AbortSignal constructor correctly illegal");
}

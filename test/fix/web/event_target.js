const et = new EventTarget();
let received = null;
et.addEventListener("greet", (e) => { received = e.type + ":" + e.detail; });

const ev = new CustomEvent("greet", { detail: "hello" });
et.dispatchEvent(ev);
console.log(received);

let onceCount = 0;
et.addEventListener("ping", () => { onceCount++; }, { once: true });
et.dispatchEvent(new Event("ping"));
et.dispatchEvent(new Event("ping"));
console.log("once count:", onceCount);

let removedCalled = false;
function removable() { removedCalled = true; }
et.addEventListener("x", removable);
et.removeEventListener("x", removable);
et.dispatchEvent(new Event("x"));
console.log("removed listener fired:", removedCalled);

const cancelable = new Event("cancel-me", { cancelable: true });
et.addEventListener("cancel-me", (e) => { e.preventDefault(); });
const notCancelled = et.dispatchEvent(cancelable);
console.log("dispatchEvent returned:", notCancelled);
console.log("defaultPrevented:", cancelable.defaultPrevented);

let order = [];
et.addEventListener("multi", () => { order.push("a"); });
et.addEventListener("multi", (e) => { order.push("b"); e.stopImmediatePropagation(); });
et.addEventListener("multi", () => { order.push("c"); });
et.dispatchEvent(new Event("multi"));
console.log("order:", order.join(","));

const handlerObj = { calls: 0, handleEvent(e) { this.calls++; } };
et.addEventListener("obj", handlerObj);
et.dispatchEvent(new Event("obj"));
console.log("handleEvent calls:", handlerObj.calls);

console.log(typeof Event, typeof CustomEvent, typeof EventTarget);

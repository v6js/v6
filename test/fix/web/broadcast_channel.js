const a = new BroadcastChannel("chan");
const b = new BroadcastChannel("chan");
const c = new BroadcastChannel("chan");
const other = new BroadcastChannel("other");

let bGot = null, cGot = null, otherGot = null;
b.onmessage = (e) => { bGot = e.data; };
c.onmessage = (e) => { cGot = e.data; };
other.onmessage = (e) => { otherGot = e.data; };

a.postMessage("hi");

setTimeout(() => {
  console.log("b:", bGot, "c:", cGot, "other:", otherGot, "name:", a.name);
  c.close();
  let cGot2 = "unset";
  c.onmessage = () => { cGot2 = "should not fire"; };
  a.postMessage("second");
  setTimeout(() => {
    console.log("after close, c saw:", cGot2, "b saw:", bGot);
    a.close();
    b.close();
    other.close();
  }, 20);
}, 20);

function dump(t) {
  const out = [];
  for (let i = 0; i < t.length; i++)
    out.push(t[i]);
  return out.join(",");
}

const a = new Uint8Array(8);
a.fill(7);
console.log(dump(a));

const b = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
b.fill(9, 2, 5);
console.log(dump(b));

const c = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
c.copyWithin(0, 4);
console.log(dump(c));

const d = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
d.copyWithin(1, 0, 3);
console.log(dump(d));

const e = new Uint8Array(8);
e.set([9, 8, 7], 2);
console.log(dump(e));

const f = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
const g = f.slice(2, 6);
console.log(dump(g));
console.log(g.length, f.length);

const h = new Uint8Array(200);
h.fill(42, 3, 190);
let sum = 0;
for (let i = 0; i < h.length; i++)
  sum += h[i];
console.log(sum);

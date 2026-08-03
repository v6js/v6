let arr = [1, 2, 3, 4];
let [a, b] = arr;
console.log(a);
console.log(b);

let [x, , z] = arr;
console.log(x);
console.log(z);

let [first, ...rest] = arr;
console.log(first);
console.log(rest.length);
console.log(rest[0]);
console.log(rest[1]);

let [p = 10, q = 20] = [1];
console.log(p);
console.log(q);

let obj = { name: "Ada", age: 36 };
let { name, age } = obj;
console.log(name);
console.log(age);

let { name: n2, city = "unknown" } = obj;
console.log(n2);
console.log(city);

function sum3(a, b, c) {
  return a + b + c;
}
console.log(sum3(...[1, 2, 3]));

function total(...nums) {
  let s = 0;
  for (let i = 0; i < nums.length; i = i + 1) {
    s = s + nums[i];
  }
  return s;
}
console.log(total(1, 2, 3, 4, 5));
console.log(total());

let more = [10, 20];
console.log(total(1, ...more, 2));

let merged = [...arr, 5, 6];
console.log(merged.length);
console.log(merged[4]);
console.log(merged[5]);

let o1 = { a: 1, b: 2 };
let o2 = { ...o1, b: 3, c: 4 };
console.log(o2.a);
console.log(o2.b);
console.log(o2.c);

let [na, [nb, nc]] = [1, [2, 3]];
console.log(na, nb, nc);

let { nx, ny: { nz, nw } } = { nx: 1, ny: { nz: 2, nw: 3 } };
console.log(nx, nz, nw);

let [np, [nq, nr] = [10, 20]] = [1];
console.log(np, nq, nr);

let { nm: [nn, no] } = { nm: [5, 6] };
console.log(nn, no);

let map1 = new Map([["x", 10], ["y", 20]]);
for (let [mk, mv] of map1.entries()) {
  console.log(mk, mv);
}

let sa = 0,
  sb = 1;
[sa, sb] = [sb, sa + sb];
console.log(sa, sb);

let dx, dy;
({ x: dx, y: dy } = { x: 1, y: 2 });
console.log(dx, dy);

let renamedTarget;
({ x: renamedTarget } = { x: 99 });
console.log(renamedTarget);

let withDefaultTarget;
({ missing: withDefaultTarget = 42 } = {});
console.log(withDefaultTarget);

let assignResult;
console.log((assignResult = ({ x: dx, y: dy } = { x: 7, y: 8 })));
console.log(dx, dy, assignResult.x, assignResult.y);

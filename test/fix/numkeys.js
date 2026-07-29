let o = { "0": "a", "1": "b", foo: "bar" };
console.log(o[0]);
console.log(o[1]);
console.log(o.foo);
console.log(Object.keys(o).length);
console.log(Object.keys(o)[0]);
console.log(Object.keys(o)[2]);

let arr = [1, 2, 3];
arr[10] = "sparse";
console.log(arr.length);
console.log(arr[10]);
console.log(arr[5]);

let frozen = [1, 2, 3];
Object.freeze(frozen);
frozen.push(4);
frozen[0] = 99;
console.log(frozen.length);
console.log(frozen[0]);

let sealed = [1, 2];
Object.seal(sealed);
sealed[0] = 100;
sealed[5] = 200;
console.log(sealed[0]);
console.log(sealed[5]);
console.log(sealed.length);

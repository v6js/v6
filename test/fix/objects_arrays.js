let person = { name: "Ada", age: 36 };
console.log(person.name);
console.log(person.age);

person.age = 37;
console.log(person.age);

person["city"] = "London";
console.log(person["city"]);
console.log(person.city);

let arr = [10, 20, 30];
console.log(arr[0]);
console.log(arr[1]);
console.log(arr[2]);
console.log(arr.length);

arr[3] = 40;
console.log(arr[3]);
console.log(arr.length);

arr[arr.length] = 50;
console.log(arr[4]);
console.log(arr.length);
console.log(arr);

let nested = { list: [1, 2, 3], info: { x: 1, y: 2 } };
console.log(nested.list[1]);
console.log(nested.info.x);

function makePoint(x, y) {
  return { x: x, y: y };
}

let p = makePoint(3, 4);
console.log(p.x + p.y);

let sum = 0;
let i = 0;
while (i < arr.length) {
  sum = sum + arr[i];
  i = i + 1;
}
console.log(sum);

let methodObj = {
  greet() {
    return "hi";
  },
};
console.log(methodObj.greet());

let shX = 5,
  shY = 10;
let shorthandObj = { shX, shY };
console.log(shorthandObj.shX);
console.log(shorthandObj.shY);

let dynKey = "dynamic";
let computedObj = { [dynKey]: 42, [dynKey + "2"]: 43 };
console.log(computedObj.dynamic);
console.log(computedObj.dynamic2);

let numKeyObj = { 0: "zero", 1: "one" };
console.log(numKeyObj[0]);
console.log(numKeyObj[1]);

let accessorObj = {
  _val: 10,
  get val() {
    return this._val * 2;
  },
  set val(v) {
    this._val = v;
  },
};
console.log(accessorObj.val);
accessorObj.val = 20;
console.log(accessorObj.val);
console.log(accessorObj._val);

let descObj = {};
Object.defineProperty(descObj, "x", { value: 5 });
console.log(descObj.x);
Object.defineProperty(descObj, "y", {
  get: function () {
    return 99;
  },
});
console.log(descObj.y);

var person = { name: "Ada", age: 36 };
console.log(person.name);
console.log(person.age);

person.age = 37;
console.log(person.age);

person["city"] = "London";
console.log(person["city"]);
console.log(person.city);

var arr = [10, 20, 30];
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

var nested = { list: [1, 2, 3], info: { x: 1, y: 2 } };
console.log(nested.list[1]);
console.log(nested.info.x);

function makePoint(x, y) {
  return { x: x, y: y };
}

var p = makePoint(3, 4);
console.log(p.x + p.y);

var sum = 0;
var i = 0;
while (i < arr.length) {
  sum = sum + arr[i];
  i = i + 1;
}
console.log(sum);

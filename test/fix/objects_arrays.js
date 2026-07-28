var person = { name: "Ada", age: 36 };
print(person.name);
print(person.age);

person.age = 37;
print(person.age);

person["city"] = "London";
print(person["city"]);
print(person.city);

var arr = [10, 20, 30];
print(arr[0]);
print(arr[1]);
print(arr[2]);
print(arr.length);

arr[3] = 40;
print(arr[3]);
print(arr.length);

arr[arr.length] = 50;
print(arr[4]);
print(arr.length);
print(arr);

var nested = { list: [1, 2, 3], info: { x: 1, y: 2 } };
print(nested.list[1]);
print(nested.info.x);

function makePoint(x, y) {
  return { x: x, y: y };
}

var p = makePoint(3, 4);
print(p.x + p.y);

var sum = 0;
var i = 0;
while (i < arr.length) {
  sum = sum + arr[i];
  i = i + 1;
}
print(sum);

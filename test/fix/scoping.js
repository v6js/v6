const pi = 3;
console.log(pi);

{
  let scoped = "inner";
  console.log(scoped);
}

{
  var leaked = 42;
}
console.log(leaked);

let outer = 1;
{
  let outer = 2;
  console.log(outer);
}
console.log(outer);

console.log(hoisted());

function hoisted() {
  return "works before declaration";
}

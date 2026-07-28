const pi = 3;
print(pi);

{
  let scoped = "inner";
  print(scoped);
}

{
  var leaked = 42;
}
print(leaked);

let outer = 1;
{
  let outer = 2;
  print(outer);
}
print(outer);

print(hoisted());

function hoisted() {
  return "works before declaration";
}

function shadowed(Math) {
  return Math.PI;
}
console.log(shadowed({ PI: 42 }));
console.log(Math.PI);

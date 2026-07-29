if (1 < 2) {
  console.log("yes");
} else {
  console.log("no");
}

if (1 > 2) {
  console.log("wrong");
} else {
  console.log("right");
}

let i = 0;
while (i < 5) {
  console.log(i);
  i = i + 1;
}

for (let j = 0; j < 3; j = j + 1) {
  console.log(j * 10);
}

console.log(true && false);
console.log(true && true);
console.log(false || true);
console.log(false || false);
console.log(!true);
console.log(!false);

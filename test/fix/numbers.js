console.log(0x1F);
console.log(0o17);
console.log(0b101);
console.log(1e3);
console.log(1.5e-2);
console.log(.5);
console.log(1 / 0);
console.log(-1 / 0);
console.log(0 / 0);

console.log(Number.isInteger(5));
console.log(Number.isInteger(5.5));
console.log(Number.isFinite(Infinity));
console.log(Number.isNaN(NaN));
console.log(Number.isSafeInteger(5));
console.log(Number.parseInt("42px"));
console.log(Number.parseFloat("3.14abc"));

console.log(parseInt("100", 2));
console.log(parseInt("0xFF"));
console.log(parseInt("-42"));
console.log(parseFloat("2.5e2"));
console.log(isNaN("hello"));
console.log(isFinite(42));

console.log(Number.MAX_SAFE_INTEGER);
console.log(Number.MIN_SAFE_INTEGER);
console.log(NaN);
console.log(Infinity);
console.log(-Infinity);

console.log((255).toString(16));
console.log((3.14159).toFixed(2));
console.log((1234.5).toFixed(0));

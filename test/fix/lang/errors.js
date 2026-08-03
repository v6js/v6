let baseError = new Error("something broke");
console.log(baseError.message);
console.log(baseError.name);
console.log(baseError.toString());
console.log(baseError instanceof Error);

let typeErr = new TypeError("bad type");
console.log(typeErr.message, typeErr.name, typeErr.toString());
console.log(typeErr instanceof TypeError);
console.log(typeErr instanceof Error);

let rangeErr = new RangeError("out of range");
console.log(rangeErr.name, rangeErr instanceof Error);

try {
  throw new TypeError("nope");
} catch (err) {
  console.log("caught:", err.toString());
  console.log(err instanceof TypeError, err instanceof Error);
}

function assertPositive(n) {
  if (n < 0) throw new RangeError("must be positive, got " + n);
  return n;
}
try {
  assertPositive(-5);
} catch (err) {
  console.log(err.name + ": " + err.message);
}

let emptyError = new Error();
console.log(emptyError.toString());
console.log(emptyError.message);

let syntaxErr = new SyntaxError("unexpected token");
let referenceErr = new ReferenceError("x is not defined");
let evalErr = new EvalError("eval failed");
let uriErr = new URIError("bad uri");
console.log(syntaxErr.name, referenceErr.name, evalErr.name, uriErr.name);
console.log(
  syntaxErr instanceof Error,
  referenceErr instanceof Error,
  evalErr instanceof Error,
  uriErr instanceof Error
);

class ValidationError extends Error {
  constructor(message, field) {
    super(message);
    this.name = "ValidationError";
    this.field = field;
  }
}
let validationErr = new ValidationError("required field missing", "email");
console.log(validationErr.toString());
console.log(validationErr.field);
console.log(validationErr instanceof Error, validationErr instanceof ValidationError);

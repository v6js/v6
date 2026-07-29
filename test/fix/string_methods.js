let s = "Hello, World!";
console.log(s.charAt(0));
console.log(s.charCodeAt(0));
console.log(s.at(-1));
console.log(s.indexOf("World"));
console.log(s.lastIndexOf("o"));
console.log(s.includes("World"));
console.log(s.startsWith("Hello"));
console.log(s.endsWith("!"));
console.log(s.slice(0, 5));
console.log(s.slice(-6));
console.log(s.substring(7, 12));
console.log(s.toUpperCase());
console.log(s.toLowerCase());

console.log("  spaced  ".trim());
console.log("  spaced  ".trimStart());
console.log("  spaced  ".trimEnd());

console.log(s.split(", ").length);
console.log(s.split(", ")[0]);
console.log("a-b-c".split("-").length);
console.log("abc".split("").length);

console.log(s.replace("World", "There"));
console.log("aaa".replaceAll("a", "b"));
console.log("ab".repeat(3));

console.log("5".padStart(3, "0"));
console.log("5".padEnd(3, "0"));
console.log("a".concat("b", "c"));

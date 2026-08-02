const fd = new FormData();
fd.append("name", "Alice");
fd.append("tag", "a");
fd.append("tag", "b");
console.log(fd.get("name"), fd.getAll("tag").join(","));
console.log(fd.has("tag"), fd.has("missing"));
fd.set("tag", "only");
console.log(fd.getAll("tag").join(","));
fd.delete("tag");
console.log(fd.has("tag"));

const blob = new Blob(["file bytes"], { type: "text/plain" });
fd.append("file", blob, "data.txt");
const fileVal = fd.get("file");
console.log(fileVal.name, fileVal.type, fileVal instanceof File);

const pairs = [];
for (const [k, v] of fd) {
  pairs.push(k + "=" + (typeof v === "string" ? v : "<file:" + v.name + ">"));
}
console.log(pairs.join(","));

const spread = [...fd];
console.log(spread.length);

const usp = new URLSearchParams("a=1&b=2&a=3");
const uspPairs = [];
for (const [k, v] of usp) {
  uspPairs.push(k + "=" + v);
}
console.log(uspPairs.join(","));
console.log([...usp].length);

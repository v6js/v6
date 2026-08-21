if (process.env.NODE_ENV === "production") {
  console.log("prod mode, debug=" + __DEBUG__);
} else {
  console.log("dev mode");
}

const ah = require("async_hooks");

console.log(typeof ah.createHook, typeof ah.executionAsyncId);
const hook = ah.createHook({});
hook.enable();
hook.disable();
console.log("hook enable/disable ok");

const { AsyncLocalStorage } = ah;
const als = new AsyncLocalStorage();
console.log(als.getStore());
const result = als.run({ userId: 42 }, () => {
  console.log("inside run:", JSON.stringify(als.getStore()));
  return "done";
});
console.log("run result:", result);
console.log("after run:", als.getStore());

const express = require("express");

const app = express();
app.use(express.json());

app.use((req, res, next) => {
  req.customHeader = req.headers["x-custom"] || null;
  next();
});

app.get("/", (req, res) => {
  res.send("Hello from v6!");
});

app.get("/users/:id", (req, res) => {
  res.json({ id: req.params.id, query: req.query });
});

app.post("/echo", (req, res) => {
  res.json({ received: req.body, custom: req.customHeader });
});

app.use((req, res) => {
  res.status(404).json({ error: "not found" });
});

const server = app.listen(0, () => {
  const port = server.address().port;
  const base = `http://localhost:${port}`;

  (async () => {
    const r1 = await fetch(base + "/");
    console.log("GET / ->", r1.status, await r1.text());

    const r2 = await fetch(base + "/users/42?sort=asc");
    console.log("GET /users/42?sort=asc ->", r2.status, await r2.json());

    const r3 = await fetch(base + "/echo", {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-Custom": "hi" },
      body: JSON.stringify({ a: 1, b: "two" }),
    });
    console.log("POST /echo ->", r3.status, await r3.json());

    const r4 = await fetch(base + "/nope");
    console.log("GET /nope ->", r4.status, await r4.json());

    server.close();
  })();
});

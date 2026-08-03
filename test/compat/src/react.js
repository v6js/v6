const React = require("react");
const ReactDOMServer = require("react-dom/cjs/react-dom-server-legacy.node.development.js");

const ThemeContext = React.createContext("light");

class Greeting extends React.Component {
  render() {
    return React.createElement("h1", { className: "greeting" }, `Hello, ${this.props.name}!`);
  }
}

function Item({ label, count }) {
  const doubled = React.useMemo(() => count * 2, [count]);
  return React.createElement("li", { key: label }, `${label}: ${count} (x2 = ${doubled})`);
}

function List({ items }) {
  return React.createElement(
    "ul",
    null,
    items.map((it) => React.createElement(Item, { key: it.label, label: it.label, count: it.count }))
  );
}

function Counter({ start }) {
  const [count, setCount] = React.useState(start);
  const theme = React.useContext(ThemeContext);

  React.useEffect(() => {
    console.log("effect ran (should NOT print during SSR)");
  }, []);

  if (count < 0) {
    return React.createElement("span", null, "negative");
  }

  return React.createElement(
    React.Fragment,
    null,
    React.createElement("p", { className: `theme-${theme}` }, `Count: ${count}`),
    React.createElement("button", { onClick: () => setCount(count + 1) }, "increment")
  );
}

function App() {
  return React.createElement(
    ThemeContext.Provider,
    { value: "dark" },
    React.createElement("div", { id: "app" },
      React.createElement(Greeting, { name: "v6" }),
      React.createElement(Counter, { start: 3 }),
      React.createElement(List, {
        items: [
          { label: "a", count: 1 },
          { label: "b", count: 2 },
          { label: "c", count: 3 },
        ],
      })
    )
  );
}

const html = ReactDOMServer.renderToStaticMarkup(React.createElement(App));
console.log("renderToStaticMarkup:", html);

const html2 = ReactDOMServer.renderToString(React.createElement(App));
console.log("renderToString:", html2);

module.exports = { html, html2 };

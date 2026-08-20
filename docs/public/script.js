(function () {
  var STORAGE_KEY = "v6-theme";
  var root = document.documentElement;
  var mql = window.matchMedia("(prefers-color-scheme: dark)");

  function currentTheme() {
    var saved = localStorage.getItem(STORAGE_KEY);
    if (saved === "light" || saved === "dark") return saved;
    return mql.matches ? "dark" : "light";
  }

  function syncHljsLinks(theme) {
    var light = document.getElementById("hljs-light");
    var dark = document.getElementById("hljs-dark");
    if (light) light.media = theme === "light" ? "all" : "not all";
    if (dark) dark.media = theme === "dark" ? "all" : "not all";
  }

  function applyTheme(theme) {
    root.setAttribute("data-theme", theme);
    syncHljsLinks(theme);
  }

  applyTheme(currentTheme());

  var btn = document.getElementById("theme-toggle");
  if (btn) {
    btn.style.display = "inline-flex";
    btn.addEventListener("click", function () {
      var next = currentTheme() === "dark" ? "light" : "dark";
      localStorage.setItem(STORAGE_KEY, next);
      applyTheme(next);
    });
  }

  mql.addEventListener("change", function () {
    if (!localStorage.getItem(STORAGE_KEY)) applyTheme(currentTheme());
  });

  var tocToggle = document.getElementById("toc-toggle");
  var tocList = document.getElementById("toc-list");
  if (tocToggle && tocList) {
    tocToggle.addEventListener("click", function () {
      var open = tocToggle.getAttribute("aria-expanded") === "true";
      tocToggle.setAttribute("aria-expanded", open ? "false" : "true");
      tocList.classList.toggle("open", !open);
    });
    tocList.querySelectorAll("a").forEach(function (a) {
      a.addEventListener("click", function () {
        tocToggle.setAttribute("aria-expanded", "false");
        tocList.classList.remove("open");
      });
    });
  }
})();

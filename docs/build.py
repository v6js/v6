import json
import pathlib
import re
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
SITE = DOCS / "site"
PAGES = DOCS / "pages"
BLOG_SRC = DOCS / "blog"
PUBLIC = DOCS / "public"
CONFIG = json.loads((DOCS / "config.json").read_text(encoding="utf-8"))

NAV_KEYS = ("NAV_HOME", "NAV_DOCS", "NAV_BLOG")

TEMPLATE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<script>(function(){{var t=localStorage.getItem("v6-theme");if(t==="light"||t==="dark")document.documentElement.setAttribute("data-theme",t);}})();</script>
<title>{title}</title>
<meta name="description" content="{description}">
<link rel="stylesheet" href="{root}style.css">
{hljs_head}</head>
<body>
<header class="site-header">
  <div class="wrap">
    <a class="brand" href="{root}index.html">{site_name}</a>
    <nav class="site-nav">
      <a href="{root}index.html" class="{nav_home}">Home</a>
      <a href="{root}docs/index.html" class="{nav_docs}">Docs</a>
      <a href="{root}blog/index.html" class="{nav_blog}">Blog</a>
      <a href="{github_url}">GitHub</a>
      <button id="theme-toggle" type="button" aria-label="Toggle color theme">
        <svg class="icon-sun" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="4"/><path d="M12 2v2"/><path d="M12 20v2"/><path d="m4.93 4.93 1.41 1.41"/><path d="m17.66 17.66 1.41 1.41"/><path d="M2 12h2"/><path d="M20 12h2"/><path d="m6.34 17.66-1.41 1.41"/><path d="m19.07 4.93-1.41 1.41"/></svg>
        <svg class="icon-moon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20.985 12.486a9 9 0 1 1-9.473-9.472c.405-.022.617.46.402.803a6 6 0 0 0 8.268 8.268c.344-.215.825-.004.803.401"/></svg>
      </button>
    </nav>
  </div>
</header>
<main>
<div class="wrap {wrap_class}">
{content}
</div>
</main>
<footer class="site-footer">
  <div class="wrap">
    <span>{site_name} is experimental software.</span>
    <a href="{github_url}">{github_label}</a>
  </div>
</footer>
{hljs_scripts}<script src="{root}script.js"></script>
</body>
</html>
"""


def strip_tags(s):
  return re.sub(r"<[^>]+>", "", s)


def fix_code_langs(html):
  html = re.sub(
      r'<pre class="([a-zA-Z0-9_+-]+)"><code>',
      lambda m: f'<pre><code class="language-{m.group(1)}">',
      html,
  )
  return html.replace("<pre><code>", '<pre><code class="language-plaintext">')


def wrap_tables(html):
  html = html.replace("<table>", '<div class="table-scroll"><table>')
  return html.replace("</table>", "</table></div>")


def md_to_html(path):
  result = subprocess.run(
      [
          "pandoc",
          "-f",
          "markdown+task_lists",
          "-t",
          "html",
          "--syntax-highlighting=none",
          "--wrap=none",
          str(path),
      ],
      capture_output=True,
      text=True,
      encoding="utf-8",
      check=True,
  )
  html = re.sub(
      r"<input\s+type=\"checkbox\"", '<input type="checkbox" disabled=""', result.stdout
  )
  html = fix_code_langs(html)
  return wrap_tables(html)


def build_sidebar_layout(html):
  matches = list(re.finditer(r'<h3 id="([^"]+)">(.*?)</h3>', html))
  if not matches:
    return html
  first = matches[0]
  html = (
      html[: first.start()]
      + f'<h3 id="{first.group(1)}" class="page-title">{first.group(2)}</h3>'
      + html[first.end() :]
  )
  if len(matches) == 1:
    return f'<div class="docs-content">{html}</div>'
  items = "\n".join(
      f'<li><a href="#{m.group(1)}">{strip_tags(m.group(2))}</a></li>'
      for m in matches[1:]
  )
  sidebar = (
      '<aside class="sidebar">'
      '<button type="button" id="toc-toggle" aria-expanded="false" aria-controls="toc-list">'
      "Contents"
      '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" '
      'stroke-linecap="round" stroke-linejoin="round"><path d="m6 9 6 6 6-6"/></svg>'
      "</button>"
      f'<ul id="toc-list">{items}</ul>'
      "</aside>"
  )
  return f'<div class="docs-layout">{sidebar}<div class="docs-content">{html}</div></div>'


def render(content, title, description, root_prefix, active, wrap_class=""):
  nav = {key: ("active" if key == active else "") for key in NAV_KEYS}
  github_url = CONFIG["github_url"]
  has_code = "<pre" in content
  hljs_head = (
      f'<link rel="stylesheet" href="{root_prefix}vendor/styles/github.min.css" media="(prefers-color-scheme: light)" id="hljs-light">\n'
      f'<link rel="stylesheet" href="{root_prefix}vendor/styles/github-dark.min.css" media="(prefers-color-scheme: dark)" id="hljs-dark">\n'
      if has_code else ""
  )
  hljs_scripts = (
      f'<script src="{root_prefix}vendor/highlight.min.js"></script>\n<script>hljs.highlightAll();</script>\n'
      if has_code else ""
  )
  return TEMPLATE.format(
      title=title,
      description=description,
      root=root_prefix,
      site_name=CONFIG["site_name"],
      github_url=github_url,
      github_label=github_url.split("://", 1)[-1],
      nav_home=nav["NAV_HOME"],
      nav_docs=nav["NAV_DOCS"],
      nav_blog=nav["NAV_BLOG"],
      content=content,
      wrap_class=wrap_class,
      hljs_head=hljs_head,
      hljs_scripts=hljs_scripts,
  )


def write_page(out_path, content, title, description, active, wrap_class=""):
  depth = len(out_path.relative_to(SITE).parts) - 1
  root_prefix = "../" * depth
  html = render(content, title, description, root_prefix, active, wrap_class)
  out_path.parent.mkdir(parents=True, exist_ok=True)
  out_path.write_text(html, encoding="utf-8")
  print(f"wrote {out_path.relative_to(ROOT)}")


def build_hero():
  hero = CONFIG["hero"]
  buttons = "\n".join(
      f'<a class="btn{"" if b["style"] == "primary" else " secondary"}" href="{b["href"]}">{b["label"]}</a>'
      for b in hero["cta"]
  )
  return (
      f'<div class="hero"><h1>{hero["title"]}</h1>'
      f'<p class="tagline">{hero["tagline"]}</p>'
      f'<div class="cta">{buttons}</div></div>'
  )


def build_home():
  body = md_to_html(PAGES / "home.md")
  content = build_hero() + body
  meta = CONFIG["pages"]["home"]
  write_page(SITE / "index.html", content, meta["title"], meta["description"], "NAV_HOME")


def build_docs():
  body = build_sidebar_layout(md_to_html(PAGES / "docs.md"))
  meta = CONFIG["pages"]["docs"]
  write_page(
      SITE / "docs" / "index.html",
      body,
      meta["title"],
      meta["description"],
      "NAV_DOCS",
      wrap_class="wide",
  )


def discover_posts():
  posts = []
  if not BLOG_SRC.is_dir():
    return posts
  for entry in sorted(BLOG_SRC.iterdir()):
    index_md = entry / "index.md"
    if entry.is_dir() and index_md.is_file():
      slug = entry.name
      date = CONFIG["posts"].get(slug, {}).get("date", "")
      posts.append((slug, date, entry, index_md))
  posts.sort(key=lambda p: p[1], reverse=True)
  return posts


def build_post(slug, date, src_dir, index_md):
  body = md_to_html(index_md)
  match = re.search(r'<h3 id="([^"]+)">(.*?)</h3>', body)
  if match:
    title_text = strip_tags(match.group(2))
    title_html = f'<h3 id="{match.group(1)}" class="page-title">{match.group(2)}</h3>'
    meta_html = f'<p class="post-meta">{date}</p>' if date else ""
    body = body[: match.start()] + title_html + meta_html + body[match.end() :]
  else:
    title_text = slug.replace("-", " ").title()
  out_dir = SITE / "blog" / slug
  write_page(out_dir / "index.html", body, f"{title_text} - {CONFIG['site_name']}", title_text, "NAV_BLOG")
  for item in src_dir.iterdir():
    if item.name == "index.md" or not item.is_file():
      continue
    shutil.copy2(item, out_dir / item.name)
  return title_text


def build_blog_index(posts):
  if posts:
    items = "\n".join(
        f'<li><p class="post-title"><a href="{slug}/index.html">{title}</a></p>'
        f'<p class="post-meta">{date}</p></li>'
        for slug, title, date in posts
    )
    content = f'<h3>Blog</h3><ul class="post-list">{items}</ul>'
  else:
    content = "<h3>Blog</h3><p>No posts yet.</p>"
  meta = CONFIG["pages"]["blog"]
  write_page(SITE / "blog" / "index.html", content, meta["title"], meta["description"], "NAV_BLOG")


def build_404():
  content = (
      '<div class="hero">'
      "<h1>404</h1>"
      '<p class="tagline">There is nothing at this address.</p>'
      '<div class="cta">'
      '<a class="btn" href="index.html">Home</a>'
      '<a class="btn secondary" href="docs/index.html">Docs</a>'
      '<a class="btn secondary" href="blog/index.html">Blog</a>'
      "</div></div>"
  )
  write_page(SITE / "404.html", content, f"404 - {CONFIG['site_name']}", "Page not found.", "")


def copy_public():
  shutil.copytree(PUBLIC, SITE, dirs_exist_ok=True)


def main():
  if SITE.exists():
    shutil.rmtree(SITE)
  SITE.mkdir(parents=True)
  copy_public()

  build_home()
  build_docs()
  build_404()
  built_posts = []
  for slug, date, src_dir, index_md in discover_posts():
    title = build_post(slug, date, src_dir, index_md)
    built_posts.append((slug, title, date))
  build_blog_index(built_posts)


if __name__ == "__main__":
  main()

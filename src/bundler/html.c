#include "v6/bundle_html.h"
#include "v6/bundle_assets.h"
#include "v6/bundle_emit.h"
#include "v6/bundle_fsutil.h"
#include "v6/bundle_graph.h"
#include "v6/bundle_strbuf.h"
#include "v6/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_whole_file(const char* path, size_t* out_len) {
  FILE* f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) {
    fclose(f);
    return NULL;
  }
  char* buf = malloc((size_t)n + 1);
  size_t got = fread(buf, 1, (size_t)n, f);
  buf[got] = '\0';
  fclose(f);
  if (out_len)
    *out_len = got;
  return buf;
}

static int find_attr_value(const char* tag, const char* tag_end,
                           const char* attr, char* out, size_t out_size) {
  size_t attr_len = strlen(attr);
  const char* p = tag;
  while (p < tag_end) {
    const char* hit = strstr(p, attr);
    if (!hit || hit >= tag_end)
      return 0;
    const char* after = hit + attr_len;
    while (after < tag_end && (*after == ' ' || *after == '\t' || *after == '\n'))
      after++;
    if (after < tag_end && *after == '=') {
      after++;
      while (after < tag_end && (*after == ' ' || *after == '\t'))
        after++;
      if (after < tag_end && (*after == '"' || *after == '\'')) {
        char quote = *after;
        after++;
        const char* val_start = after;
        const char* val_end = memchr(val_start, quote, (size_t)(tag_end - val_start));
        if (val_end) {
          size_t len = (size_t)(val_end - val_start);
          if (len >= out_size)
            len = out_size - 1;
          memcpy(out, val_start, len);
          out[len] = '\0';
          return 1;
        }
      }
    }
    p = hit + 1;
  }
  return 0;
}

static bundle_format script_format_for_tag(const char* tag, const char* tag_end) {
  char type[64];
  if (find_attr_value(tag, tag_end, "type", type, sizeof(type)) &&
      strcmp(type, "module") == 0) {
    return bundle_fmt_esm;
  }
  return bundle_fmt_iife;
}

static int bundle_one_script(const char* entry_path, const char* outdir,
                             bundle_format fmt, const char* global_name,
                             char* out_url, size_t out_url_size) {
  bundle_graph g;
  bundle_graph_init(&g);
  int rc = bundle_graph_build(&g, entry_path);
  if (rc != 0) {
    for (int i = 0; i < g.error_count; i++)
      fprintf(stderr, "error: %s\n", g.errors[i]);
    bundle_graph_free(&g);
    return -1;
  }

  if (bundle_process_assets(&g, outdir) != 0) {
    fprintf(stderr, "error: failed to write asset files for %s\n", entry_path);
    bundle_graph_free(&g);
    return -1;
  }

  bundle_emit_options eopts;
  eopts.format = fmt;
  eopts.global_name = global_name;
  size_t out_len = 0;
  char* output = bundle_emit(&g, &eopts, &out_len);

  const char* base = strrchr(entry_path, '/');
  const char* bbase = strrchr(entry_path, '\\');
  if (bbase && (!base || bbase > base))
    base = bbase;
  base = base ? base + 1 : entry_path;

  char stem[512];
  const char* dot = strrchr(base, '.');
  size_t stem_len = dot ? (size_t)(dot - base) : strlen(base);
  if (stem_len >= sizeof(stem))
    stem_len = sizeof(stem) - 1;
  memcpy(stem, base, stem_len);
  stem[stem_len] = '\0';

  unsigned long long hash = bundle_fnv1a(output, out_len);
  char out_name[600];
  snprintf(out_name, sizeof(out_name), "%s.%08llx.js", stem, hash & 0xffffffffULL);

  char dst_path[1200];
  snprintf(dst_path, sizeof(dst_path), "%s/%s", outdir, out_name);
  int wrc = bundle_write_file(dst_path, output, out_len);

  free(output);
  bundle_graph_free(&g);

  if (wrc != 0) {
    fprintf(stderr, "error: cannot write %s\n", dst_path);
    return -1;
  }

  snprintf(out_url, out_url_size, "%s", out_name);
  return 0;
}

int bundle_process_html(const char* html_path, const char* outdir,
                        const char* global_name) {
  size_t html_len = 0;
  char* html = read_whole_file(html_path, &html_len);
  if (!html) {
    fprintf(stderr, "error: cannot read %s\n", html_path);
    return 1;
  }

  char html_dir[1024];
  path_dirname(html_path, html_dir, sizeof(html_dir));

  bundle_mkdir_p(outdir);

  bundle_strbuf out;
  bundle_strbuf_init(&out);

  char* cursor = html;
  int had_error = 0;

  while (*cursor) {
    char* script_hit = strstr(cursor, "<script");
    char* link_hit = strstr(cursor, "<link");
    char* hit = NULL;
    int is_script = 0;

    if (script_hit && (!link_hit || script_hit < link_hit)) {
      hit = script_hit;
      is_script = 1;
    } else if (link_hit) {
      hit = link_hit;
      is_script = 0;
    }

    if (!hit) {
      bundle_strbuf_append_cstr(&out, cursor);
      break;
    }

    char* tag_end = strchr(hit, '>');
    if (!tag_end) {
      bundle_strbuf_append_cstr(&out, cursor);
      break;
    }
    tag_end++;

    bundle_strbuf_append(&out, cursor, (size_t)(hit - cursor));

    char src[1024];
    int handled = 0;
    char* advance_to = tag_end;

    if (is_script && find_attr_value(hit, tag_end, "src", src, sizeof(src))) {
      char entry_path[1200];
      snprintf(entry_path, sizeof(entry_path), "%s/%s", html_dir, src);
      bundle_format fmt = script_format_for_tag(hit, tag_end);
      char out_name[600];
      if (bundle_one_script(entry_path, outdir, fmt, global_name, out_name,
                            sizeof(out_name)) == 0) {
        bundle_strbuf_append_fmt(&out, "<script src=\"%s\"%s></script>",
                                 out_name,
                                 fmt == bundle_fmt_esm ? " type=\"module\"" : "");
        handled = 1;
        char* close_tag = strstr(tag_end, "</script>");
        if (close_tag)
          advance_to = close_tag + strlen("</script>");
      } else {
        had_error = 1;
      }
    } else if (!is_script) {
      char rel[64];
      char href[1024];
      if (find_attr_value(hit, tag_end, "rel", rel, sizeof(rel)) &&
          strcmp(rel, "stylesheet") == 0 &&
          find_attr_value(hit, tag_end, "href", href, sizeof(href))) {
        char css_path[1200];
        snprintf(css_path, sizeof(css_path), "%s/%s", html_dir, href);
        size_t css_len = 0;
        char* css = read_whole_file(css_path, &css_len);
        if (css) {
          unsigned long long hash = bundle_fnv1a(css, css_len);
          const char* base = strrchr(href, '/');
          base = base ? base + 1 : href;
          char stem[400];
          const char* dot = strrchr(base, '.');
          size_t stem_len = dot ? (size_t)(dot - base) : strlen(base);
          if (stem_len >= sizeof(stem))
            stem_len = sizeof(stem) - 1;
          memcpy(stem, base, stem_len);
          stem[stem_len] = '\0';
          char out_name[500];
          snprintf(out_name, sizeof(out_name), "%s.%08llx.css", stem,
                   hash & 0xffffffffULL);
          char assets_dir[1100];
          snprintf(assets_dir, sizeof(assets_dir), "%s/assets", outdir);
          bundle_mkdir_p(assets_dir);
          char dst_path[1300];
          snprintf(dst_path, sizeof(dst_path), "%s/%s", assets_dir, out_name);
          bundle_write_file(dst_path, css, css_len);
          free(css);
          bundle_strbuf_append_fmt(
              &out, "<link rel=\"stylesheet\" href=\"assets/%s\">", out_name);
          handled = 1;
        }
      }
    }

    if (!handled)
      bundle_strbuf_append(&out, hit, (size_t)(tag_end - hit));

    cursor = advance_to;
  }

  free(html);

  const char* base = strrchr(html_path, '/');
  const char* bbase = strrchr(html_path, '\\');
  if (bbase && (!base || bbase > base))
    base = bbase;
  base = base ? base + 1 : html_path;

  char dst_html[1200];
  snprintf(dst_html, sizeof(dst_html), "%s/%s", outdir, base);
  size_t out_len = 0;
  char* out_data = bundle_strbuf_take(&out, &out_len);
  int wrc = bundle_write_file(dst_html, out_data, out_len);
  free(out_data);

  if (wrc != 0) {
    fprintf(stderr, "error: cannot write %s\n", dst_html);
    return 1;
  }

  printf("wrote %s\n", dst_html);
  return had_error ? 1 : 0;
}

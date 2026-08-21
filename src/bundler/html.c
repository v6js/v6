#include "v6/bundler_html.h"
#include "v6/bundler_assets.h"
#include "v6/bundler_emit.h"
#include "v6/bundler_fsutil.h"
#include "v6/bundler_graph.h"
#include "v6/bundler_strbuf.h"
#include "v6/module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define v6_getcwd _getcwd
#else
#include <unistd.h>
#define v6_getcwd getcwd
#endif

int v6_bundler_html_resolve_root(const char* path, char* out, size_t out_size) {
  char cwd[v6_max_path];
  if (!v6_getcwd(cwd, sizeof(cwd)))
    return -1;
  char qualified[v6_max_path];
  const char* for_resolve = path;
  int looks_relative = path[0] == '.' || path[0] == '/' || path[0] == '\\' ||
                       (path[0] != '\0' && path[1] == ':');
  if (!looks_relative) {
    snprintf(qualified, sizeof(qualified), "./%s", path);
    for_resolve = qualified;
  }
  char resolved[v6_max_path];
  char err[256];
  if (resolve_module_specifier(cwd, for_resolve, resolved, sizeof(resolved),
                               err, sizeof(err)) != 0)
    return -1;
  path_dirname(resolved, out, out_size);
  return 0;
}

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
    while (after < tag_end &&
           (*after == ' ' || *after == '\t' || *after == '\n'))
      after++;
    if (after < tag_end && *after == '=') {
      after++;
      while (after < tag_end && (*after == ' ' || *after == '\t'))
        after++;
      if (after < tag_end && (*after == '"' || *after == '\'')) {
        char quote = *after;
        after++;
        const char* val_start = after;
        const char* val_end =
            memchr(val_start, quote, (size_t)(tag_end - val_start));
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

int v6_bundler_html_scan_scripts(const char* html_path,
                                 v6_bundler_html_script_ref* out_scripts,
                                 int max_scripts, int* out_count) {
  size_t html_len = 0;
  char* html = read_whole_file(html_path, &html_len);
  if (!html)
    return -1;

  char html_dir[1024];
  path_dirname(html_path, html_dir, sizeof(html_dir));

  int count = 0;
  char* cursor = html;
  while (*cursor && count < max_scripts) {
    char* hit = strstr(cursor, "<script");
    if (!hit)
      break;
    char* tag_end = strchr(hit, '>');
    if (!tag_end)
      break;
    tag_end++;

    char src[1024];
    if (find_attr_value(hit, tag_end, "src", src, sizeof(src))) {
      snprintf(out_scripts[count].entry_path,
               sizeof(out_scripts[count].entry_path), "%s/%s", html_dir, src);
      char type[64];
      out_scripts[count].is_module =
          find_attr_value(hit, tag_end, "type", type, sizeof(type)) &&
          strcmp(type, "module") == 0;
      count++;
    }
    cursor = tag_end;
  }

  free(html);
  *out_count = count;
  return 0;
}

static const char* basename_of(const char* path) {
  const char* slash = strrchr(path, '/');
  const char* bslash = strrchr(path, '\\');
  const char* last = slash;
  if (bslash && (!last || bslash > last))
    last = bslash;
  return last ? last + 1 : path;
}

static void stem_of(const char* base, char* out, size_t out_size) {
  const char* dot = strrchr(base, '.');
  size_t len = dot ? (size_t)(dot - base) : strlen(base);
  if (len >= out_size)
    len = out_size - 1;
  memcpy(out, base, len);
  out[len] = '\0';
}

static int path_in_set(const char* path, char shared[][1200],
                       int shared_count) {
  for (int i = 0; i < shared_count; i++) {
    if (strcmp(path, shared[i]) == 0)
      return 1;
  }
  return 0;
}

int v6_bundler_process_html(const char* html_path, const char* outdir,
                            const char* global_name, int dev_mode,
                            v6_bundler_extension_set* extensions) {
  size_t html_len = 0;
  char* html = read_whole_file(html_path, &html_len);
  if (!html) {
    fprintf(stderr, "error: cannot read %s\n", html_path);
    return 1;
  }

  char html_dir[1024];
  if (v6_bundler_html_resolve_root(html_path, html_dir, sizeof(html_dir)) !=
      0) {
    fprintf(stderr, "error: cannot resolve %s\n", html_path);
    free(html);
    return 1;
  }
  v6_bundler_mkdir_p(outdir);

  v6_bundler_html_script_ref scripts[v6_bundler_html_max_scripts];
  int script_count = 0;
  v6_bundler_html_scan_scripts(html_path, scripts, v6_bundler_html_max_scripts,
                               &script_count);

  v6_bundler_graph graphs[v6_bundler_html_max_scripts];
  int graph_ok[v6_bundler_html_max_scripts];
  int had_error = 0;
  for (int i = 0; i < script_count; i++) {
    v6_bundler_graph_init(&graphs[i]);
    graphs[i].extensions = extensions;
    graph_ok[i] = v6_bundler_graph_build_with_root(
                      &graphs[i], scripts[i].entry_path, html_dir) == 0;
    if (!graph_ok[i]) {
      for (int e = 0; e < graphs[i].error_count; e++)
        fprintf(stderr, "error: %s\n", graphs[i].errors[e]);
      had_error = 1;
    }
  }

  char shared_paths[64][1200];
  int shared_count = 0;
  for (int i = 0; i < script_count; i++) {
    if (!graph_ok[i])
      continue;
    for (int mi = 0; mi < graphs[i].count; mi++) {
      const char* path = graphs[i].modules[mi]->abs_path;
      if (path_in_set(path, shared_paths, shared_count))
        continue;
      int occ = 0;
      for (int j = 0; j < script_count; j++) {
        if (!graph_ok[j])
          continue;
        for (int mj = 0; mj < graphs[j].count; mj++) {
          if (strcmp(graphs[j].modules[mj]->abs_path, path) == 0) {
            occ++;
            break;
          }
        }
      }
      if (occ >= 2 && shared_count < 64) {
        snprintf(shared_paths[shared_count], sizeof(shared_paths[0]), "%s",
                 path);
        shared_count++;
      }
    }
  }

  char shared_chunk_name[600] = "";
  if (shared_count > 0) {
    v6_bundler_strbuf sb;
    v6_bundler_strbuf_init(&sb);
    v6_bundler_emit_runtime_preamble(&sb);
    for (int s = 0; s < shared_count; s++) {
      for (int i = 0; i < script_count; i++) {
        if (!graph_ok[i])
          continue;
        v6_bundler_module* found = NULL;
        for (int mi = 0; mi < graphs[i].count; mi++) {
          if (strcmp(graphs[i].modules[mi]->abs_path, shared_paths[s]) == 0) {
            found = graphs[i].modules[mi];
            break;
          }
        }
        if (found) {
          v6_bundler_emit_one_module(&sb, found);
          break;
        }
      }
    }
    size_t shared_len = 0;
    char* shared_output = v6_bundler_strbuf_take(&sb, &shared_len);
    shared_output = v6_bundler_extension_run_finalize(extensions, shared_output,
                                                      shared_len, &shared_len);
    unsigned long long hash = v6_bundler_fnv1a(shared_output, shared_len);
    snprintf(shared_chunk_name, sizeof(shared_chunk_name), "shared.%08llx.js",
             hash & 0xffffffffULL);
    char dst[1200];
    snprintf(dst, sizeof(dst), "%s/%s", outdir, shared_chunk_name);
    v6_bundler_write_file(dst, shared_output, shared_len);
    free(shared_output);
  }

  int use_dev_format = dev_mode || shared_count > 0;

  char script_out_names[v6_bundler_html_max_scripts][600];
  int script_is_module[v6_bundler_html_max_scripts];
  for (int i = 0; i < script_count; i++) {
    script_out_names[i][0] = '\0';
    script_is_module[i] = scripts[i].is_module;
    if (!graph_ok[i])
      continue;

    v6_bundler_process_assets(&graphs[i], outdir);

    char* output;
    size_t out_len = 0;
    if (use_dev_format) {
      v6_bundler_strbuf sb;
      v6_bundler_strbuf_init(&sb);
      v6_bundler_emit_runtime_preamble(&sb);
      v6_bundler_module** order;
      int order_count;
      v6_bundler_graph_topo_order(&graphs[i], &order, &order_count);
      for (int oi = 0; oi < order_count; oi++) {
        if (!path_in_set(order[oi]->abs_path, shared_paths, shared_count))
          v6_bundler_emit_one_module(&sb, order[oi]);
      }
      free(order);
      v6_bundler_emit_entry_require(&sb, graphs[i].entry, global_name);
      output = v6_bundler_strbuf_take(&sb, &out_len);
      script_is_module[i] = 0;
    } else {
      v6_bundler_emit_options eopts;
      eopts.format =
          scripts[i].is_module ? v6_bundler_fmt_esm : v6_bundler_fmt_iife;
      eopts.global_name = global_name;
      output = v6_bundler_emit(&graphs[i], &eopts, &out_len);
    }

    output = v6_bundler_extension_run_finalize(extensions, output, out_len,
                                               &out_len);

    char stem[512];
    stem_of(basename_of(scripts[i].entry_path), stem, sizeof(stem));
    unsigned long long hash = v6_bundler_fnv1a(output, out_len);
    snprintf(script_out_names[i], sizeof(script_out_names[i]), "%s.%08llx.js",
             stem, hash & 0xffffffffULL);

    char dst_path[1200];
    snprintf(dst_path, sizeof(dst_path), "%s/%s", outdir, script_out_names[i]);
    int wrc = v6_bundler_write_file(dst_path, output, out_len);
    free(output);
    if (wrc != 0) {
      fprintf(stderr, "error: cannot write %s\n", dst_path);
      had_error = 1;
      script_out_names[i][0] = '\0';
    }
  }

  for (int i = 0; i < script_count; i++)
    v6_bundler_graph_free(&graphs[i]);

  v6_bundler_strbuf out;
  v6_bundler_strbuf_init(&out);

  char* cursor = html;
  int script_index = 0;
  int shared_injected = 0;

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
      v6_bundler_strbuf_append_cstr(&out, cursor);
      break;
    }

    char* tag_end = strchr(hit, '>');
    if (!tag_end) {
      v6_bundler_strbuf_append_cstr(&out, cursor);
      break;
    }
    tag_end++;

    v6_bundler_strbuf_append(&out, cursor, (size_t)(hit - cursor));

    char src[1024];
    int handled = 0;
    char* advance_to = tag_end;

    if (is_script && find_attr_value(hit, tag_end, "src", src, sizeof(src))) {
      int idx = script_index++;
      if (idx < script_count && script_out_names[idx][0]) {
        if (shared_chunk_name[0] && !shared_injected) {
          v6_bundler_strbuf_append_fmt(&out, "<script src=\"%s\"></script>",
                                       shared_chunk_name);
          shared_injected = 1;
        }
        v6_bundler_strbuf_append_fmt(
            &out, "<script src=\"%s\"%s></script>", script_out_names[idx],
            script_is_module[idx] ? " type=\"module\"" : "");
        handled = 1;
      }
      char* close_tag = strstr(tag_end, "</script>");
      if (close_tag)
        advance_to = close_tag + strlen("</script>");
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
          unsigned long long hash = v6_bundler_fnv1a(css, css_len);
          char stem[400];
          stem_of(basename_of(href), stem, sizeof(stem));
          char out_name[500];
          snprintf(out_name, sizeof(out_name), "%s.%08llx.css", stem,
                   hash & 0xffffffffULL);
          char assets_dir[1100];
          snprintf(assets_dir, sizeof(assets_dir), "%s/assets", outdir);
          v6_bundler_mkdir_p(assets_dir);
          char dst_path[1300];
          snprintf(dst_path, sizeof(dst_path), "%s/%s", assets_dir, out_name);
          v6_bundler_write_file(dst_path, css, css_len);
          free(css);
          v6_bundler_strbuf_append_fmt(
              &out, "<link rel=\"stylesheet\" href=\"assets/%s\">", out_name);
          handled = 1;
        }
      }
    }

    if (!handled)
      v6_bundler_strbuf_append(&out, hit, (size_t)(tag_end - hit));

    cursor = advance_to;
  }

  free(html);

  char dst_html[1200];
  snprintf(dst_html, sizeof(dst_html), "%s/%s", outdir, basename_of(html_path));
  size_t out_len = 0;
  char* out_data = v6_bundler_strbuf_take(&out, &out_len);
  int wrc = v6_bundler_write_file(dst_html, out_data, out_len);
  free(out_data);

  if (wrc != 0) {
    fprintf(stderr, "error: cannot write %s\n", dst_html);
    return 1;
  }

  v6_bundler_extension_run_emit(extensions, outdir);

  return had_error ? 1 : 0;
}

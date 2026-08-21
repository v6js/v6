#include "v6/bundler_scan.h"

#include <string.h>

static void push_spec(v6_bundler_arena* a, v6_bundler_specifier_list* out,
                      const char* text, size_t len, int is_require) {
  if (out->len >= out->cap) {
    int new_cap = out->cap == 0 ? 8 : out->cap * 2;
    v6_bundler_specifier* items =
        v6_bundler_arena_alloc(a, sizeof(v6_bundler_specifier) * new_cap);
    if (out->items)
      memcpy(items, out->items, sizeof(v6_bundler_specifier) * out->len);
    out->items = items;
    out->cap = new_cap;
  }
  out->items[out->len].text = text;
  out->items[out->len].len = len;
  out->items[out->len].is_require = is_require;
  out->len++;
}

static void walk(v6_bundler_arena* a, ast_node* n,
                 v6_bundler_specifier_list* out);

static void walk_list(v6_bundler_arena* a, ast_list* list,
                      v6_bundler_specifier_list* out) {
  for (int i = 0; i < list->len; i++)
    walk(a, list->items[i], out);
}

static void walk(v6_bundler_arena* a, ast_node* n,
                 v6_bundler_specifier_list* out) {
  if (!n)
    return;

  if (n->kind == ast_import && n->str && n->str_len > 0) {
    push_spec(a, out, n->str, n->str_len, 0);
  } else if (n->kind == ast_call && n->a && n->a->kind == ast_ident &&
             n->a->str_len == 7 && memcmp(n->a->str, "require", 7) == 0 &&
             n->list.len == 1 && n->list.items[0]->kind == ast_str) {
    push_spec(a, out, n->list.items[0]->str, n->list.items[0]->str_len, 1);
  }

  walk(a, n->a, out);
  walk(a, n->b, out);
  walk(a, n->c, out);
  walk(a, n->d, out);
  walk_list(a, &n->list, out);
  walk_list(a, &n->quasis_cooked, out);
  walk_list(a, &n->quasis_raw, out);

  for (int i = 0; i < n->props.len; i++) {
    walk(a, n->props.items[i].key, out);
    walk(a, n->props.items[i].value, out);
  }
  for (int i = 0; i < n->members.len; i++) {
    walk(a, n->members.items[i].key, out);
    walk(a, n->members.items[i].value, out);
  }
  for (int i = 0; i < n->params.len; i++) {
    walk(a, n->params.items[i].pattern, out);
  }
  for (int i = 0; i < n->cases.len; i++) {
    walk(a, n->cases.items[i].test, out);
    walk_list(a, &n->cases.items[i].body, out);
  }
}

void v6_bundler_scan_imports(v6_bundler_arena* out_arena, ast_node* program,
                             v6_bundler_specifier_list* out) {
  out->items = NULL;
  out->len = 0;
  out->cap = 0;
  walk(out_arena, program, out);
}

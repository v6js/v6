#include "v6/bundle_scan.h"

#include <string.h>

static void push_spec(bundle_arena* a, bundle_specifier_list* out,
                      const char* text, size_t len, int is_require) {
  if (out->len >= out->cap) {
    int new_cap = out->cap == 0 ? 8 : out->cap * 2;
    bundle_specifier* items =
        bundle_arena_alloc(a, sizeof(bundle_specifier) * new_cap);
    if (out->items)
      memcpy(items, out->items, sizeof(bundle_specifier) * out->len);
    out->items = items;
    out->cap = new_cap;
  }
  out->items[out->len].text = text;
  out->items[out->len].len = len;
  out->items[out->len].is_require = is_require;
  out->len++;
}

static void walk(bundle_arena* a, ast_node* n, bundle_specifier_list* out);

static void walk_list(bundle_arena* a, ast_list* list,
                      bundle_specifier_list* out) {
  for (int i = 0; i < list->len; i++)
    walk(a, list->items[i], out);
}

static void walk(bundle_arena* a, ast_node* n, bundle_specifier_list* out) {
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

void bundle_scan_imports(bundle_arena* out_arena, ast_node* program,
                         bundle_specifier_list* out) {
  out->items = NULL;
  out->len = 0;
  out->cap = 0;
  walk(out_arena, program, out);
}

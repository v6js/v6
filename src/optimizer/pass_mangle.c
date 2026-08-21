#include "v6/optimizer_pass.h"

#include <string.h>

static int is_reserved_or_unsafe(const char* s, size_t len) {
  static const char* words[] = {
      "in",        "of",         "do",         "if",        "for",
      "let",       "new",        "try",        "var",       "case",
      "else",      "enum",       "eval",       "null",      "this",
      "true",      "void",       "with",       "break",     "catch",
      "class",     "const",      "false",      "super",     "throw",
      "while",     "yield",      "delete",     "export",    "import",
      "public",    "return",     "static",     "switch",    "typeof",
      "default",   "extends",    "finally",    "package",   "private",
      "continue",  "debugger",   "function",   "arguments", "interface",
      "protected", "implements", "instanceof", "undefined", "NaN",
      "Infinity",  NULL,
  };
  for (int i = 0; words[i]; i++) {
    if (strlen(words[i]) == len && memcmp(words[i], s, len) == 0)
      return 1;
  }
  return 0;
}

static void gen_name_from_index(int idx, char* out) {
  char buf[16];
  int len = 0;
  int n = idx;
  do {
    buf[len++] = (char)('a' + (n % 26));
    n = n / 26 - 1;
  } while (n >= 0);
  for (int i = 0; i < len; i++)
    out[i] = buf[len - 1 - i];
  out[len] = '\0';
}

#define mangle_max_locals 512

typedef struct local_entry {
  const char* name;
  size_t len;
} local_entry;

typedef struct local_set {
  local_entry items[mangle_max_locals];
  int count;
} local_set;

static void set_add(local_set* set, const char* name, size_t len) {
  for (int i = 0; i < set->count; i++) {
    if (set->items[i].len == len && memcmp(set->items[i].name, name, len) == 0)
      return;
  }
  if (set->count < mangle_max_locals) {
    set->items[set->count].name = name;
    set->items[set->count].len = len;
    set->count++;
  }
}

static void collect_pattern_names(ast_node* pat, local_set* out) {
  if (!pat)
    return;
  switch (pat->kind) {
  case ast_pat_ident:
    set_add(out, pat->str, pat->str_len);
    break;
  case ast_pat_assign:
    collect_pattern_names(pat->a, out);
    break;
  case ast_pat_rest:
    collect_pattern_names(pat->a, out);
    break;
  case ast_pat_array:
    for (int i = 0; i < pat->list.len; i++)
      collect_pattern_names(pat->list.items[i], out);
    break;
  case ast_pat_object:
    for (int i = 0; i < pat->props.len; i++)
      collect_pattern_names(pat->props.items[i].value, out);
    break;
  default:
    break;
  }
}

static void collect_own_locals_stmt(ast_node* n, local_set* out);
static void collect_own_locals_expr_shallow(ast_node* n, local_set* out);

static void collect_own_locals_list(ast_list* list, local_set* out) {
  for (int i = 0; i < list->len; i++)
    collect_own_locals_stmt(list->items[i], out);
}

static void collect_own_locals_var_decl(ast_node* n, local_set* out) {
  for (int i = 0; i < n->list.len; i++) {
    ast_node* d = n->list.items[i];
    ast_node* target = d->kind == ast_pat_assign ? d->a : d;
    collect_pattern_names(target, out);
  }
}

static void collect_own_locals_stmt(ast_node* n, local_set* out) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_block:
    collect_own_locals_list(&n->list, out);
    break;
  case ast_var_decl:
    collect_own_locals_var_decl(n, out);
    break;
  case ast_func_decl:
    break;
  case ast_class_decl:
    break;
  case ast_if:
    collect_own_locals_stmt(n->b, out);
    collect_own_locals_stmt(n->c, out);
    break;
  case ast_while:
    collect_own_locals_stmt(n->b, out);
    break;
  case ast_do_while:
    collect_own_locals_stmt(n->a, out);
    break;
  case ast_for:
    if (n->a && n->a->kind == ast_var_decl)
      collect_own_locals_var_decl(n->a, out);
    collect_own_locals_stmt(n->d, out);
    break;
  case ast_for_in:
  case ast_for_of:
    if (n->flag_a != tok_eof)
      collect_pattern_names(n->a, out);
    collect_own_locals_stmt(n->c, out);
    break;
  case ast_switch:
    for (int i = 0; i < n->cases.len; i++)
      collect_own_locals_list(&n->cases.items[i].body, out);
    break;
  case ast_try:
    collect_own_locals_stmt(n->a, out);
    if (n->flag_a && n->b)
      collect_pattern_names(n->b, out);
    collect_own_locals_stmt(n->c, out);
    collect_own_locals_stmt(n->d, out);
    break;
  case ast_labeled:
    collect_own_locals_stmt(n->a, out);
    break;
  case ast_expr_stmt:
    collect_own_locals_expr_shallow(n->a, out);
    break;
  default:
    break;
  }
}

static void collect_own_locals_expr_shallow(ast_node* n, local_set* out) {
  (void)n;
  (void)out;
}

static void decl_count_stmt(ast_node* n, const char* name, size_t len,
                            int* count);
static void decl_count_expr(ast_node* n, const char* name, size_t len,
                            int* count);

static void decl_count_pattern(ast_node* pat, const char* name, size_t len,
                               int* count) {
  if (!pat)
    return;
  switch (pat->kind) {
  case ast_pat_ident:
    if (pat->str_len == len && memcmp(pat->str, name, len) == 0)
      (*count)++;
    break;
  case ast_pat_assign:
    decl_count_pattern(pat->a, name, len, count);
    decl_count_expr(pat->b, name, len, count);
    break;
  case ast_pat_rest:
    decl_count_pattern(pat->a, name, len, count);
    break;
  case ast_pat_array:
    for (int i = 0; i < pat->list.len; i++)
      decl_count_pattern(pat->list.items[i], name, len, count);
    break;
  case ast_pat_object:
    for (int i = 0; i < pat->props.len; i++)
      decl_count_pattern(pat->props.items[i].value, name, len, count);
    break;
  default:
    break;
  }
}

static void decl_count_params(ast_param_list* params, const char* name,
                              size_t len, int* count) {
  for (int i = 0; i < params->len; i++)
    decl_count_pattern(params->items[i].pattern, name, len, count);
}

static void decl_count_expr(ast_node* n, const char* name, size_t len,
                            int* count) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    decl_count_expr(n->a, name, len, count);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    decl_count_expr(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    break;
  case ast_cond:
    decl_count_expr(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    decl_count_expr(n->c, name, len, count);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      decl_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        decl_count_expr(p->key, name, len, count);
      decl_count_expr(p->value, name, len, count);
    }
    break;
  case ast_member:
    decl_count_expr(n->a, name, len, count);
    if (n->flag_a)
      decl_count_expr(n->b, name, len, count);
    break;
  case ast_call:
  case ast_new:
    decl_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->list.len; i++)
      decl_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_tagged_template:
    decl_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->list.len; i++)
      decl_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_func_expr:
    decl_count_params(&n->params, name, len, count);
    if (n->str_len == len && memcmp(n->str, name, len) == 0)
      (*count)++;
    if (n->flag_d)
      decl_count_stmt(n->a, name, len, count);
    else
      decl_count_expr(n->a, name, len, count);
    break;
  case ast_class_expr:
    if (n->flag_a)
      decl_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        decl_count_expr(m->key, name, len, count);
      if (m->value)
        decl_count_expr(m->value, name, len, count);
    }
    break;
  case ast_paren_pattern_assign:
    decl_count_pattern(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    break;
  default:
    break;
  }
}

static void decl_count_stmt(ast_node* n, const char* name, size_t len,
                            int* count) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      decl_count_stmt(n->list.items[i], name, len, count);
    break;
  case ast_expr_stmt:
    decl_count_expr(n->a, name, len, count);
    break;
  case ast_var_decl:
    for (int i = 0; i < n->list.len; i++) {
      ast_node* d = n->list.items[i];
      ast_node* target = d->kind == ast_pat_assign ? d->a : d;
      decl_count_pattern(target, name, len, count);
      if (d->kind == ast_pat_assign)
        decl_count_expr(d->b, name, len, count);
    }
    break;
  case ast_func_decl:
    if (n->str_len == len && memcmp(n->str, name, len) == 0)
      (*count)++;
    decl_count_params(&n->params, name, len, count);
    decl_count_stmt(n->a, name, len, count);
    break;
  case ast_class_decl:
    if (n->str_len == len && memcmp(n->str, name, len) == 0)
      (*count)++;
    decl_count_expr(n, name, len, count);
    break;
  case ast_if:
    decl_count_expr(n->a, name, len, count);
    decl_count_stmt(n->b, name, len, count);
    decl_count_stmt(n->c, name, len, count);
    break;
  case ast_while:
    decl_count_expr(n->a, name, len, count);
    decl_count_stmt(n->b, name, len, count);
    break;
  case ast_do_while:
    decl_count_stmt(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        decl_count_stmt(n->a, name, len, count);
      else
        decl_count_expr(n->a, name, len, count);
    }
    decl_count_expr(n->b, name, len, count);
    decl_count_expr(n->c, name, len, count);
    decl_count_stmt(n->d, name, len, count);
    break;
  case ast_for_in:
  case ast_for_of:
    if (n->flag_a != tok_eof)
      decl_count_pattern(n->a, name, len, count);
    decl_count_expr(n->b, name, len, count);
    decl_count_stmt(n->c, name, len, count);
    break;
  case ast_switch:
    decl_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        decl_count_expr(n->cases.items[i].test, name, len, count);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        decl_count_stmt(n->cases.items[i].body.items[j], name, len, count);
    }
    break;
  case ast_try:
    decl_count_stmt(n->a, name, len, count);
    if (n->flag_a && n->b)
      decl_count_pattern(n->b, name, len, count);
    decl_count_stmt(n->c, name, len, count);
    decl_count_stmt(n->d, name, len, count);
    break;
  case ast_throw:
  case ast_return:
    decl_count_expr(n->a, name, len, count);
    break;
  case ast_labeled:
    decl_count_stmt(n->a, name, len, count);
    break;
  default:
    break;
  }
}

static void use_count_expr(ast_node* n, const char* name, size_t len,
                           int* count);
static void use_count_stmt(ast_node* n, const char* name, size_t len,
                           int* count);

static void use_count_expr(ast_node* n, const char* name, size_t len,
                           int* count) {
  if (!n)
    return;
  if (n->kind == ast_ident) {
    if (n->str_len == len && memcmp(n->str, name, len) == 0)
      (*count)++;
    return;
  }
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    use_count_expr(n->a, name, len, count);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    use_count_expr(n->a, name, len, count);
    use_count_expr(n->b, name, len, count);
    break;
  case ast_cond:
    use_count_expr(n->a, name, len, count);
    use_count_expr(n->b, name, len, count);
    use_count_expr(n->c, name, len, count);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      use_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        use_count_expr(p->key, name, len, count);
      use_count_expr(p->value, name, len, count);
    }
    break;
  case ast_member:
    use_count_expr(n->a, name, len, count);
    if (n->flag_a)
      use_count_expr(n->b, name, len, count);
    break;
  case ast_call:
  case ast_new:
    use_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->list.len; i++)
      use_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_tagged_template:
    use_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->list.len; i++)
      use_count_expr(n->list.items[i], name, len, count);
    break;
  case ast_func_expr:
    if (n->flag_d)
      use_count_stmt(n->a, name, len, count);
    else
      use_count_expr(n->a, name, len, count);
    break;
  case ast_class_expr:
    if (n->flag_a)
      use_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        use_count_expr(m->key, name, len, count);
      if (m->value)
        use_count_expr(m->value, name, len, count);
    }
    break;
  case ast_paren_pattern_assign:
    use_count_expr(n->b, name, len, count);
    break;
  default:
    break;
  }
}

static void use_count_stmt(ast_node* n, const char* name, size_t len,
                           int* count) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      use_count_stmt(n->list.items[i], name, len, count);
    break;
  case ast_expr_stmt:
    use_count_expr(n->a, name, len, count);
    break;
  case ast_var_decl:
    for (int i = 0; i < n->list.len; i++) {
      ast_node* d = n->list.items[i];
      if (d->kind == ast_pat_assign)
        use_count_expr(d->b, name, len, count);
    }
    break;
  case ast_func_decl:
    use_count_stmt(n->a, name, len, count);
    break;
  case ast_class_decl:
    use_count_expr(n, name, len, count);
    break;
  case ast_if:
    use_count_expr(n->a, name, len, count);
    use_count_stmt(n->b, name, len, count);
    use_count_stmt(n->c, name, len, count);
    break;
  case ast_while:
    use_count_expr(n->a, name, len, count);
    use_count_stmt(n->b, name, len, count);
    break;
  case ast_do_while:
    use_count_stmt(n->a, name, len, count);
    use_count_expr(n->b, name, len, count);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        use_count_stmt(n->a, name, len, count);
      else
        use_count_expr(n->a, name, len, count);
    }
    use_count_expr(n->b, name, len, count);
    use_count_expr(n->c, name, len, count);
    use_count_stmt(n->d, name, len, count);
    break;
  case ast_for_in:
  case ast_for_of:
    use_count_expr(n->b, name, len, count);
    use_count_stmt(n->c, name, len, count);
    break;
  case ast_switch:
    use_count_expr(n->a, name, len, count);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        use_count_expr(n->cases.items[i].test, name, len, count);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        use_count_stmt(n->cases.items[i].body.items[j], name, len, count);
    }
    break;
  case ast_try:
    use_count_stmt(n->a, name, len, count);
    use_count_stmt(n->c, name, len, count);
    use_count_stmt(n->d, name, len, count);
    break;
  case ast_throw:
  case ast_return:
    use_count_expr(n->a, name, len, count);
    break;
  case ast_labeled:
    use_count_stmt(n->a, name, len, count);
    break;
  default:
    break;
  }
}

static int name_appears_anywhere(ast_node* scope_root, const char* name,
                                 size_t len) {
  int count = 0;
  decl_count_stmt(scope_root, name, len, &count);
  if (count == 0)
    use_count_stmt(scope_root, name, len, &count);
  return count > 0;
}

static void rename_pattern(ast_node* pat, const char* old, size_t old_len,
                           const char* new_name, size_t new_len) {
  if (!pat)
    return;
  switch (pat->kind) {
  case ast_pat_ident:
    if (pat->str_len == old_len && memcmp(pat->str, old, old_len) == 0) {
      pat->str = new_name;
      pat->str_len = new_len;
    }
    break;
  case ast_pat_assign:
    rename_pattern(pat->a, old, old_len, new_name, new_len);
    break;
  case ast_pat_rest:
    rename_pattern(pat->a, old, old_len, new_name, new_len);
    break;
  case ast_pat_array:
    for (int i = 0; i < pat->list.len; i++)
      rename_pattern(pat->list.items[i], old, old_len, new_name, new_len);
    break;
  case ast_pat_object:
    for (int i = 0; i < pat->props.len; i++)
      rename_pattern(pat->props.items[i].value, old, old_len, new_name,
                     new_len);
    break;
  default:
    break;
  }
}

static void rename_expr(ast_node* n, const char* old, size_t old_len,
                        const char* new_name, size_t new_len);
static void rename_stmt(ast_node* n, const char* old, size_t old_len,
                        const char* new_name, size_t new_len);

static void rename_expr(ast_node* n, const char* old, size_t old_len,
                        const char* new_name, size_t new_len) {
  if (!n)
    return;
  if (n->kind == ast_ident) {
    if (n->str_len == old_len && memcmp(n->str, old, old_len) == 0) {
      n->str = new_name;
      n->str_len = new_len;
    }
    return;
  }
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    rename_expr(n->a, old, old_len, new_name, new_len);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    rename_expr(n->a, old, old_len, new_name, new_len);
    rename_expr(n->b, old, old_len, new_name, new_len);
    break;
  case ast_cond:
    rename_expr(n->a, old, old_len, new_name, new_len);
    rename_expr(n->b, old, old_len, new_name, new_len);
    rename_expr(n->c, old, old_len, new_name, new_len);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    for (int i = 0; i < n->list.len; i++)
      rename_expr(n->list.items[i], old, old_len, new_name, new_len);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        rename_expr(p->key, old, old_len, new_name, new_len);
      rename_expr(p->value, old, old_len, new_name, new_len);
    }
    break;
  case ast_member:
    rename_expr(n->a, old, old_len, new_name, new_len);
    if (n->flag_a)
      rename_expr(n->b, old, old_len, new_name, new_len);
    break;
  case ast_call:
  case ast_new:
    rename_expr(n->a, old, old_len, new_name, new_len);
    for (int i = 0; i < n->list.len; i++)
      rename_expr(n->list.items[i], old, old_len, new_name, new_len);
    break;
  case ast_tagged_template:
    rename_expr(n->a, old, old_len, new_name, new_len);
    for (int i = 0; i < n->list.len; i++)
      rename_expr(n->list.items[i], old, old_len, new_name, new_len);
    break;
  case ast_func_expr:
    for (int i = 0; i < n->params.len; i++)
      rename_pattern(n->params.items[i].pattern, old, old_len, new_name,
                     new_len);
    if (n->str_len == old_len && memcmp(n->str, old, old_len) == 0) {
      n->str = new_name;
      n->str_len = new_len;
    }
    if (n->flag_d)
      rename_stmt(n->a, old, old_len, new_name, new_len);
    else
      rename_expr(n->a, old, old_len, new_name, new_len);
    break;
  case ast_class_expr:
    if (n->flag_a)
      rename_expr(n->a, old, old_len, new_name, new_len);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        rename_expr(m->key, old, old_len, new_name, new_len);
      if (m->value)
        rename_expr(m->value, old, old_len, new_name, new_len);
    }
    break;
  case ast_paren_pattern_assign:
    rename_pattern(n->a, old, old_len, new_name, new_len);
    rename_expr(n->b, old, old_len, new_name, new_len);
    break;
  default:
    break;
  }
}

static void rename_stmt(ast_node* n, const char* old, size_t old_len,
                        const char* new_name, size_t new_len) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    for (int i = 0; i < n->list.len; i++)
      rename_stmt(n->list.items[i], old, old_len, new_name, new_len);
    break;
  case ast_expr_stmt:
    rename_expr(n->a, old, old_len, new_name, new_len);
    break;
  case ast_var_decl:
    for (int i = 0; i < n->list.len; i++) {
      ast_node* d = n->list.items[i];
      ast_node* target = d->kind == ast_pat_assign ? d->a : d;
      rename_pattern(target, old, old_len, new_name, new_len);
      if (d->kind == ast_pat_assign)
        rename_expr(d->b, old, old_len, new_name, new_len);
    }
    break;
  case ast_func_decl:
    if (n->str_len == old_len && memcmp(n->str, old, old_len) == 0) {
      n->str = new_name;
      n->str_len = new_len;
    }
    for (int i = 0; i < n->params.len; i++)
      rename_pattern(n->params.items[i].pattern, old, old_len, new_name,
                     new_len);
    rename_stmt(n->a, old, old_len, new_name, new_len);
    break;
  case ast_class_decl:
    if (n->str_len == old_len && memcmp(n->str, old, old_len) == 0) {
      n->str = new_name;
      n->str_len = new_len;
    }
    rename_expr(n, old, old_len, new_name, new_len);
    break;
  case ast_if:
    rename_expr(n->a, old, old_len, new_name, new_len);
    rename_stmt(n->b, old, old_len, new_name, new_len);
    rename_stmt(n->c, old, old_len, new_name, new_len);
    break;
  case ast_while:
    rename_expr(n->a, old, old_len, new_name, new_len);
    rename_stmt(n->b, old, old_len, new_name, new_len);
    break;
  case ast_do_while:
    rename_stmt(n->a, old, old_len, new_name, new_len);
    rename_expr(n->b, old, old_len, new_name, new_len);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        rename_stmt(n->a, old, old_len, new_name, new_len);
      else
        rename_expr(n->a, old, old_len, new_name, new_len);
    }
    rename_expr(n->b, old, old_len, new_name, new_len);
    rename_expr(n->c, old, old_len, new_name, new_len);
    rename_stmt(n->d, old, old_len, new_name, new_len);
    break;
  case ast_for_in:
  case ast_for_of:
    if (n->flag_a != tok_eof)
      rename_pattern(n->a, old, old_len, new_name, new_len);
    rename_expr(n->b, old, old_len, new_name, new_len);
    rename_stmt(n->c, old, old_len, new_name, new_len);
    break;
  case ast_switch:
    rename_expr(n->a, old, old_len, new_name, new_len);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        rename_expr(n->cases.items[i].test, old, old_len, new_name, new_len);
      for (int j = 0; j < n->cases.items[i].body.len; j++)
        rename_stmt(n->cases.items[i].body.items[j], old, old_len, new_name,
                    new_len);
    }
    break;
  case ast_try:
    rename_stmt(n->a, old, old_len, new_name, new_len);
    if (n->flag_a && n->b)
      rename_pattern(n->b, old, old_len, new_name, new_len);
    rename_stmt(n->c, old, old_len, new_name, new_len);
    rename_stmt(n->d, old, old_len, new_name, new_len);
    break;
  case ast_throw:
  case ast_return:
    rename_expr(n->a, old, old_len, new_name, new_len);
    break;
  case ast_labeled:
    rename_stmt(n->a, old, old_len, new_name, new_len);
    break;
  default:
    break;
  }
}

static void mangle_function_scope(ast_arena* arena, ast_param_list* params,
                                  ast_node* body) {
  local_set candidates;
  candidates.count = 0;
  if (params) {
    for (int i = 0; i < params->len; i++)
      collect_pattern_names(params->items[i].pattern, &candidates);
  }
  collect_own_locals_stmt(body, &candidates);

  int next_idx = 0;
  for (int i = 0; i < candidates.count; i++) {
    const char* name = candidates.items[i].name;
    size_t len = candidates.items[i].len;

    int decl_total = 0;
    if (params) {
      for (int j = 0; j < params->len; j++)
        decl_count_pattern(params->items[j].pattern, name, len, &decl_total);
    }
    decl_count_stmt(body, name, len, &decl_total);
    if (decl_total != 1)
      continue;

    char candidate[16];
    const char* new_name = NULL;
    size_t new_len = 0;
    for (;; next_idx++) {
      gen_name_from_index(next_idx, candidate);
      size_t cand_len = strlen(candidate);
      if (is_reserved_or_unsafe(candidate, cand_len))
        continue;
      if (cand_len == len && memcmp(candidate, name, len) == 0) {
        next_idx++;
        continue;
      }
      int exists = 0;
      if (params) {
        for (int j = 0; j < params->len && !exists; j++) {
          int c = 0;
          decl_count_pattern(params->items[j].pattern, candidate, cand_len, &c);
          exists = c > 0;
        }
      }
      if (!exists && name_appears_anywhere(body, candidate, cand_len))
        exists = 1;
      if (exists)
        continue;
      new_name = ast_arena_strdup(arena, candidate, cand_len);
      new_len = cand_len;
      next_idx++;
      break;
    }

    if (params) {
      for (int j = 0; j < params->len; j++)
        rename_pattern(params->items[j].pattern, name, len, new_name, new_len);
    }
    rename_stmt(body, name, len, new_name, new_len);
  }
}

static void mangle_stmt(ast_arena* arena, ast_node* n);
static void mangle_expr(ast_arena* arena, ast_node* n);

static void mangle_list(ast_arena* arena, ast_list* list) {
  for (int i = 0; i < list->len; i++)
    mangle_stmt(arena, list->items[i]);
}

static void mangle_function_like(ast_arena* arena, ast_node* n) {
  mangle_function_scope(arena, &n->params, n->flag_d ? n->a : NULL);
  if (n->flag_d)
    mangle_stmt(arena, n->a);
  else
    mangle_expr(arena, n->a);
}

static void mangle_expr(ast_arena* arena, ast_node* n) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_unary:
  case ast_spread:
  case ast_await:
  case ast_yield:
  case ast_update:
    mangle_expr(arena, n->a);
    break;
  case ast_binary:
  case ast_logical:
  case ast_assign:
    mangle_expr(arena, n->a);
    mangle_expr(arena, n->b);
    break;
  case ast_cond:
    mangle_expr(arena, n->a);
    mangle_expr(arena, n->b);
    mangle_expr(arena, n->c);
    break;
  case ast_seq:
  case ast_array_lit:
  case ast_template:
    mangle_list(arena, &n->list);
    break;
  case ast_object_lit:
    for (int i = 0; i < n->props.len; i++) {
      ast_prop* p = &n->props.items[i];
      if (p->computed)
        mangle_expr(arena, p->key);
      mangle_expr(arena, p->value);
    }
    break;
  case ast_member:
    mangle_expr(arena, n->a);
    if (n->flag_a)
      mangle_expr(arena, n->b);
    break;
  case ast_call:
  case ast_new:
    mangle_expr(arena, n->a);
    mangle_list(arena, &n->list);
    break;
  case ast_tagged_template:
    mangle_expr(arena, n->a);
    mangle_list(arena, &n->list);
    break;
  case ast_func_expr:
    mangle_function_like(arena, n);
    break;
  case ast_class_expr:
    if (n->flag_a)
      mangle_expr(arena, n->a);
    for (int i = 0; i < n->members.len; i++) {
      ast_class_member* m = &n->members.items[i];
      if (m->computed)
        mangle_expr(arena, m->key);
      if (m->value)
        mangle_expr(arena, m->value);
    }
    break;
  case ast_paren_pattern_assign:
    mangle_expr(arena, n->b);
    break;
  default:
    break;
  }
}

static void mangle_stmt(ast_arena* arena, ast_node* n) {
  if (!n)
    return;
  switch (n->kind) {
  case ast_program:
  case ast_block:
    mangle_list(arena, &n->list);
    break;
  case ast_expr_stmt:
    mangle_expr(arena, n->a);
    break;
  case ast_var_decl:
    for (int i = 0; i < n->list.len; i++) {
      ast_node* d = n->list.items[i];
      if (d->kind == ast_pat_assign)
        mangle_expr(arena, d->b);
    }
    break;
  case ast_func_decl:
    mangle_function_like(arena, n);
    break;
  case ast_class_decl:
    mangle_expr(arena, n);
    break;
  case ast_if:
    mangle_expr(arena, n->a);
    mangle_stmt(arena, n->b);
    mangle_stmt(arena, n->c);
    break;
  case ast_while:
    mangle_expr(arena, n->a);
    mangle_stmt(arena, n->b);
    break;
  case ast_do_while:
    mangle_stmt(arena, n->a);
    mangle_expr(arena, n->b);
    break;
  case ast_for:
    if (n->a) {
      if (n->a->kind == ast_var_decl)
        mangle_stmt(arena, n->a);
      else
        mangle_expr(arena, n->a);
    }
    mangle_expr(arena, n->b);
    mangle_expr(arena, n->c);
    mangle_stmt(arena, n->d);
    break;
  case ast_for_in:
  case ast_for_of:
    mangle_expr(arena, n->b);
    mangle_stmt(arena, n->c);
    break;
  case ast_switch:
    mangle_expr(arena, n->a);
    for (int i = 0; i < n->cases.len; i++) {
      if (n->cases.items[i].test)
        mangle_expr(arena, n->cases.items[i].test);
      mangle_list(arena, &n->cases.items[i].body);
    }
    break;
  case ast_try:
    mangle_stmt(arena, n->a);
    mangle_stmt(arena, n->c);
    mangle_stmt(arena, n->d);
    break;
  case ast_throw:
  case ast_return:
    mangle_expr(arena, n->a);
    break;
  case ast_labeled:
    mangle_stmt(arena, n->a);
    break;
  default:
    break;
  }
}

void v6_opt_pass_mangle(ast_node* program, ast_arena* arena) {
  mangle_stmt(arena, program);
}

#include "v6/ast.h"

#include <stdlib.h>
#include <string.h>

#define ast_arena_block_size (256 * 1024)

void ast_arena_init(ast_arena* a) {
  a->head = NULL;
}

void ast_arena_free(ast_arena* a) {
  ast_arena_block* b = a->head;
  while (b) {
    ast_arena_block* next = b->next;
    free(b);
    b = next;
  }
  a->head = NULL;
}

static ast_arena_block* ast_arena_new_block(size_t min_cap) {
  size_t cap = ast_arena_block_size;
  if (min_cap > cap)
    cap = min_cap;
  ast_arena_block* b = malloc(sizeof(ast_arena_block) + cap);
  b->next = NULL;
  b->cap = cap;
  b->used = 0;
  return b;
}

void* ast_arena_alloc(ast_arena* a, size_t size) {
  size = (size + 7) & ~(size_t)7;
  if (!a->head || a->head->used + size > a->head->cap) {
    ast_arena_block* b = ast_arena_new_block(size);
    b->next = a->head;
    a->head = b;
  }
  void* p = a->head->data + a->head->used;
  a->head->used += size;
  return p;
}

char* ast_arena_strdup(ast_arena* a, const char* s, size_t len) {
  char* out = ast_arena_alloc(a, len + 1);
  memcpy(out, s, len);
  out[len] = '\0';
  return out;
}

ast_node* ast_new_node(ast_arena* a, ast_kind kind, int line) {
  ast_node* n = ast_arena_alloc(a, sizeof(ast_node));
  memset(n, 0, sizeof(ast_node));
  n->kind = kind;
  n->line = line;
  return n;
}

void ast_list_push(ast_arena* a, ast_list* list, ast_node* node) {
  if (list->len >= list->cap) {
    int new_cap = list->cap == 0 ? 4 : list->cap * 2;
    ast_node** new_items = ast_arena_alloc(a, sizeof(ast_node*) * new_cap);
    if (list->items)
      memcpy(new_items, list->items, sizeof(ast_node*) * list->len);
    list->items = new_items;
    list->cap = new_cap;
  }
  list->items[list->len++] = node;
}

void ast_prop_list_push(ast_arena* a, ast_prop_list* list, ast_prop prop) {
  if (list->len >= list->cap) {
    int new_cap = list->cap == 0 ? 4 : list->cap * 2;
    ast_prop* new_items = ast_arena_alloc(a, sizeof(ast_prop) * new_cap);
    if (list->items)
      memcpy(new_items, list->items, sizeof(ast_prop) * list->len);
    list->items = new_items;
    list->cap = new_cap;
  }
  list->items[list->len++] = prop;
}

void ast_class_member_list_push(ast_arena* a, ast_class_member_list* list,
                                ast_class_member member) {
  if (list->len >= list->cap) {
    int new_cap = list->cap == 0 ? 4 : list->cap * 2;
    ast_class_member* new_items =
        ast_arena_alloc(a, sizeof(ast_class_member) * new_cap);
    if (list->items)
      memcpy(new_items, list->items, sizeof(ast_class_member) * list->len);
    list->items = new_items;
    list->cap = new_cap;
  }
  list->items[list->len++] = member;
}

void ast_param_list_push(ast_arena* a, ast_param_list* list, ast_param param) {
  if (list->len >= list->cap) {
    int new_cap = list->cap == 0 ? 4 : list->cap * 2;
    ast_param* new_items = ast_arena_alloc(a, sizeof(ast_param) * new_cap);
    if (list->items)
      memcpy(new_items, list->items, sizeof(ast_param) * list->len);
    list->items = new_items;
    list->cap = new_cap;
  }
  list->items[list->len++] = param;
}

void ast_switch_case_list_push(ast_arena* a, ast_switch_case_list* list,
                               ast_switch_case c) {
  if (list->len >= list->cap) {
    int new_cap = list->cap == 0 ? 4 : list->cap * 2;
    ast_switch_case* new_items =
        ast_arena_alloc(a, sizeof(ast_switch_case) * new_cap);
    if (list->items)
      memcpy(new_items, list->items, sizeof(ast_switch_case) * list->len);
    list->items = new_items;
    list->cap = new_cap;
  }
  list->items[list->len++] = c;
}

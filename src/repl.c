#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "v6/repl.h"

#include "v6/bytecode.h"
#include "v6/color.h"
#include "v6/daemon.h"
#include "v6/jar.h"
#include "v6/lexer.h"
#include "v6/module.h"
#include "v6/parser.h"
#include "v6/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <io.h>
#include <windows.h>
#define v6_dup _dup
#define v6_dup2 _dup2
#define v6_fileno _fileno
#else
#include <termios.h>
#include <unistd.h>
#define v6_dup dup
#define v6_dup2 dup2
#define v6_fileno fileno
#endif

#define V6_REPL_LINE_CAP 8192
#define V6_REPL_HISTORY_CAP 1000

typedef enum {
  key_char,
  key_enter,
  key_backspace,
  key_delete,
  key_left,
  key_right,
  key_up,
  key_down,
  key_home,
  key_end,
  key_ctrl_c,
  key_ctrl_d,
  key_tab,
  key_other,
} key_kind;

typedef struct {
  key_kind kind;
  char ch;
} key_event;

#ifdef _WIN32
static key_event read_key(void) {
  int c = _getch();
  if (c == 3)
    return (key_event){key_ctrl_c, 0};
  if (c == 4)
    return (key_event){key_ctrl_d, 0};
  if (c == '\r' || c == '\n')
    return (key_event){key_enter, 0};
  if (c == 8 || c == 127)
    return (key_event){key_backspace, 0};
  if (c == '\t')
    return (key_event){key_tab, 0};
  if (c == 0 || c == 0xE0) {
    int c2 = _getch();
    switch (c2) {
    case 72:
      return (key_event){key_up, 0};
    case 80:
      return (key_event){key_down, 0};
    case 75:
      return (key_event){key_left, 0};
    case 77:
      return (key_event){key_right, 0};
    case 83:
      return (key_event){key_delete, 0};
    case 71:
      return (key_event){key_home, 0};
    case 79:
      return (key_event){key_end, 0};
    default:
      return (key_event){key_other, 0};
    }
  }
  if (c >= 32 && c < 127)
    return (key_event){key_char, (char)c};
  return (key_event){key_other, 0};
}

static void enable_raw_mode(void) {
}

static void disable_raw_mode(void) {
}
#else
static struct termios g_orig_termios;
static int g_raw_active = 0;

static void disable_raw_mode(void) {
  if (g_raw_active) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
    g_raw_active = 0;
  }
}

static void enable_raw_mode(void) {
  if (tcgetattr(STDIN_FILENO, &g_orig_termios) != 0)
    return;
  struct termios raw = g_orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  g_raw_active = 1;
}

static key_event read_key(void) {
  unsigned char c;
  if (read(STDIN_FILENO, &c, 1) <= 0)
    return (key_event){key_ctrl_d, 0};
  if (c == 3)
    return (key_event){key_ctrl_c, 0};
  if (c == 4)
    return (key_event){key_ctrl_d, 0};
  if (c == '\r' || c == '\n')
    return (key_event){key_enter, 0};
  if (c == 127 || c == 8)
    return (key_event){key_backspace, 0};
  if (c == '\t')
    return (key_event){key_tab, 0};
  if (c == 0x1b) {
    unsigned char seq[2];
    if (read(STDIN_FILENO, &seq[0], 1) <= 0)
      return (key_event){key_other, 0};
    if (read(STDIN_FILENO, &seq[1], 1) <= 0)
      return (key_event){key_other, 0};
    if (seq[0] == '[') {
      switch (seq[1]) {
      case 'A':
        return (key_event){key_up, 0};
      case 'B':
        return (key_event){key_down, 0};
      case 'C':
        return (key_event){key_right, 0};
      case 'D':
        return (key_event){key_left, 0};
      case 'H':
        return (key_event){key_home, 0};
      case 'F':
        return (key_event){key_end, 0};
      case '3': {
        unsigned char tilde;
        read(STDIN_FILENO, &tilde, 1);
        return (key_event){key_delete, 0};
      }
      default:
        return (key_event){key_other, 0};
      }
    }
    return (key_event){key_other, 0};
  }
  if (c >= 32 && c < 127)
    return (key_event){key_char, (char)c};
  return (key_event){key_other, 0};
}
#endif

static char* v6_repl_strdup(const char* s) {
  size_t n = strlen(s) + 1;
  char* out = malloc(n);
  memcpy(out, s, n);
  return out;
}

typedef struct {
  char* items[V6_REPL_HISTORY_CAP];
  int count;
  char path[1024];
} repl_history;

static void history_path(char* out, size_t cap) {
#ifdef _WIN32
  const char* home = getenv("USERPROFILE");
#else
  const char* home = getenv("HOME");
#endif
  if (home && home[0])
    snprintf(out, cap, "%s/.v6_repl_history", home);
  else
    out[0] = '\0';
}

static void history_load(repl_history* h) {
  history_path(h->path, sizeof(h->path));
  h->count = 0;
  if (!h->path[0])
    return;
  FILE* f = fopen(h->path, "rb");
  if (!f)
    return;
  char line[V6_REPL_LINE_CAP];
  while (fgets(line, sizeof(line), f) && h->count < V6_REPL_HISTORY_CAP) {
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
      line[--n] = '\0';
    if (n == 0)
      continue;
    h->items[h->count++] = v6_repl_strdup(line);
  }
  fclose(f);
}

static void history_add(repl_history* h, const char* line) {
  if (line[0] == '\0')
    return;
  if (h->count > 0 && strcmp(h->items[h->count - 1], line) == 0)
    return;
  if (h->count == V6_REPL_HISTORY_CAP) {
    free(h->items[0]);
    memmove(h->items, h->items + 1, sizeof(char*) * (size_t)(h->count - 1));
    h->count--;
  }
  h->items[h->count++] = v6_repl_strdup(line);
}

static void history_save(repl_history* h) {
  if (!h->path[0])
    return;
  FILE* f = fopen(h->path, "wb");
  if (!f)
    return;
  int start = h->count > 500 ? h->count - 500 : 0;
  for (int i = start; i < h->count; i++)
    fprintf(f, "%s\n", h->items[i]);
  fclose(f);
}

typedef struct {
  char buf[V6_REPL_LINE_CAP];
  size_t len;
  size_t cursor;
} line_buf;

static void redraw(const char* prompt, int color, line_buf* lb) {
  printf("\r\x1b[K%s%s%s", v6_c_cyan(color), prompt, v6_c_reset(color));
  fwrite(lb->buf, 1, lb->len, stdout);
  if (lb->len > lb->cursor)
    printf("\x1b[%zuD", lb->len - lb->cursor);
  fflush(stdout);
}

static void lb_insert(line_buf* lb, char c) {
  if (lb->len + 1 >= sizeof(lb->buf))
    return;
  memmove(lb->buf + lb->cursor + 1, lb->buf + lb->cursor, lb->len - lb->cursor);
  lb->buf[lb->cursor] = c;
  lb->len++;
  lb->cursor++;
}

static void lb_backspace(line_buf* lb) {
  if (lb->cursor == 0)
    return;
  memmove(lb->buf + lb->cursor - 1, lb->buf + lb->cursor, lb->len - lb->cursor);
  lb->len--;
  lb->cursor--;
}

static void lb_delete_forward(line_buf* lb) {
  if (lb->cursor >= lb->len)
    return;
  memmove(lb->buf + lb->cursor, lb->buf + lb->cursor + 1,
          lb->len - lb->cursor - 1);
  lb->len--;
}

static void lb_set(line_buf* lb, const char* s) {
  size_t n = strlen(s);
  if (n >= sizeof(lb->buf))
    n = sizeof(lb->buf) - 1;
  memcpy(lb->buf, s, n);
  lb->len = n;
  lb->cursor = n;
}

typedef enum {
  read_ok,
  read_eof,
  read_interrupted,
  read_exit_requested,
} read_status;

static int stdin_is_tty(void) {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

static read_status read_line_piped(const char* prompt, int color, char* out,
                                   size_t out_cap) {
  printf("%s%s%s", v6_c_cyan(color), prompt, v6_c_reset(color));
  fflush(stdout);
  if (!fgets(out, (int)out_cap, stdin))
    return read_eof;
  size_t n = strlen(out);
  while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r'))
    out[--n] = '\0';
  printf("%s\n", out);
  return read_ok;
}

static read_status read_line_interactive(const char* prompt, int color,
                                         repl_history* hist, char* out,
                                         size_t out_cap, int at_top_level) {
  line_buf lb = {{0}, 0, 0};
  char draft[V6_REPL_LINE_CAP] = {0};
  int hist_idx = hist->count;
  int ctrl_c_armed = 0;

  redraw(prompt, color, &lb);
  for (;;) {
    key_event k = read_key();
    switch (k.kind) {
    case key_ctrl_c:
      if (lb.len > 0 || !at_top_level) {
        printf("\n");
        return read_interrupted;
      }
      if (ctrl_c_armed) {
        printf("\n");
        return read_exit_requested;
      }
      ctrl_c_armed = 1;
      printf("\n(To exit, press Ctrl+C again or Ctrl+D or type .exit)\n");
      redraw(prompt, color, &lb);
      break;
    case key_ctrl_d:
      if (lb.len == 0) {
        printf("\n");
        return read_eof;
      }
      lb_delete_forward(&lb);
      redraw(prompt, color, &lb);
      break;
    case key_enter:
      printf("\n");
      lb.buf[lb.len] = '\0';
      strncpy(out, lb.buf, out_cap - 1);
      out[out_cap - 1] = '\0';
      return read_ok;
    case key_backspace:
      lb_backspace(&lb);
      redraw(prompt, color, &lb);
      break;
    case key_delete:
      lb_delete_forward(&lb);
      redraw(prompt, color, &lb);
      break;
    case key_left:
      if (lb.cursor > 0)
        lb.cursor--;
      redraw(prompt, color, &lb);
      break;
    case key_right:
      if (lb.cursor < lb.len)
        lb.cursor++;
      redraw(prompt, color, &lb);
      break;
    case key_home:
      lb.cursor = 0;
      redraw(prompt, color, &lb);
      break;
    case key_end:
      lb.cursor = lb.len;
      redraw(prompt, color, &lb);
      break;
    case key_up:
      if (hist->count > 0 && hist_idx > 0) {
        if (hist_idx == hist->count) {
          lb.buf[lb.len] = '\0';
          strncpy(draft, lb.buf, sizeof(draft) - 1);
        }
        hist_idx--;
        lb_set(&lb, hist->items[hist_idx]);
        redraw(prompt, color, &lb);
      }
      break;
    case key_down:
      if (hist_idx < hist->count) {
        hist_idx++;
        lb_set(&lb, hist_idx == hist->count ? draft : hist->items[hist_idx]);
        redraw(prompt, color, &lb);
      }
      break;
    case key_tab:
      break;
    case key_char:
      lb_insert(&lb, k.ch);
      redraw(prompt, color, &lb);
      break;
    default:
      break;
    }
  }
}

static const char* const STATEMENT_PREFIXES[] = {
    "const ",  "let ",    "var ", "function ", "function*", "class ",
    "if ",     "if(",     "for ", "for(",      "while ",    "while(",
    "switch ", "switch(", "try ", "try{",      "import ",   "export ",
    "return",  "{",       "//",   "async ",    NULL,
};

static int looks_like_statement(const char* line) {
  for (int i = 0; STATEMENT_PREFIXES[i]; i++) {
    if (strncmp(line, STATEMENT_PREFIXES[i], strlen(STATEMENT_PREFIXES[i])) ==
        0)
      return 1;
  }
  return 0;
}

static void wrap_line(const char* line, char* out, size_t out_cap) {
  if (looks_like_statement(line)) {
    snprintf(out, out_cap, "%s", line);
    return;
  }
  size_t n = strlen(line);
  char expr[V6_REPL_LINE_CAP];
  size_t elen = n;
  if (elen >= sizeof(expr))
    elen = sizeof(expr) - 1;
  memcpy(expr, line, elen);
  expr[elen] = '\0';
  if (elen > 0 && expr[elen - 1] == ';')
    expr[elen - 1] = '\0';
  snprintf(out, out_cap, "__v6ReplEcho(%s);", expr);
}

static int input_is_balanced(const char* src) {
  lexer lx;
  lex_init(&lx, src);
  int depth = 0;
  for (;;) {
    tok t = lex_next(&lx);
    if (t.kind == tok_eof)
      return depth <= 0;
    if (t.kind == tok_error)
      return 1;
    if (t.kind == tok_lparen || t.kind == tok_lbrace || t.kind == tok_lbracket)
      depth++;
    else if (t.kind == tok_rparen || t.kind == tok_rbrace ||
             t.kind == tok_rbracket)
      depth--;
  }
}

static int try_compile(const char* src, const char* in_path, buf* out_bytes,
                       module_ctx* modctx, char* err_msg, size_t err_cap,
                       int* err_line) {
  class_file cf;
  cf_init(&cf, "Main", "java/lang/Object");
  compile_result rc = compile_program(src, &cf, in_path, modctx);
  if (!rc.ok) {
    snprintf(err_msg, err_cap, "%s", rc.message);
    *err_line = rc.line;
    cf_free(&cf);
    return 0;
  }
  buf_init(out_bytes);
  cf_emit(&cf, out_bytes);
  cf_free(&cf);
  return 1;
}

static char* capture_and_run(v6_cli_options* opts, buf* out_bytes,
                             module_ctx* modctx, int* exit_code,
                             size_t* out_len) {
  fflush(stdout);
  fflush(stderr);
  FILE* tmp = tmpfile();
  if (!tmp) {
    *exit_code = 1;
    *out_len = 0;
    return NULL;
  }
  int saved_out = v6_dup(v6_fileno(stdout));
  int saved_err = v6_dup(v6_fileno(stderr));
  v6_dup2(v6_fileno(tmp), v6_fileno(stdout));
  v6_dup2(v6_fileno(tmp), v6_fileno(stderr));

  int num_classes = 1 + modctx->count;
  v6_daemon_class_entry* classes =
      malloc(sizeof(v6_daemon_class_entry) * (size_t)num_classes);
  classes[0].name = "Main";
  classes[0].data = out_bytes->data;
  classes[0].len = out_bytes->len;
  buf* mod_bufs =
      modctx->count > 0 ? malloc(sizeof(buf) * (size_t)modctx->count) : NULL;
  for (int i = 0; i < modctx->count; i++) {
    buf_init(&mod_bufs[i]);
    cf_emit(modctx->modules[i].cf, &mod_bufs[i]);
    cf_free(modctx->modules[i].cf);
    free(modctx->modules[i].cf);
    classes[i + 1].name = modctx->modules[i].class_name;
    classes[i + 1].data = mod_bufs[i].data;
    classes[i + 1].len = mod_bufs[i].len;
  }

  v6_daemon_run(opts->prog, classes, num_classes, "[repl]", NULL, 0,
                v6_color_enabled_out(), exit_code);

  free(classes);
  if (mod_bufs) {
    for (int i = 0; i < modctx->count; i++)
      buf_free(&mod_bufs[i]);
    free(mod_bufs);
  }

  fflush(stdout);
  fflush(stderr);
  v6_dup2(saved_out, v6_fileno(stdout));
  v6_dup2(saved_err, v6_fileno(stderr));
#ifdef _WIN32
  _close(saved_out);
  _close(saved_err);
#else
  close(saved_out);
  close(saved_err);
#endif

  long len = ftell(tmp);
  if (len < 0)
    len = 0;
  fseek(tmp, 0, SEEK_SET);
  char* data = malloc((size_t)len + 1);
  size_t rd = fread(data, 1, (size_t)len, tmp);
  data[rd] = '\0';
  fclose(tmp);
  *out_len = rd;
  return data;
}

static void print_help(int color) {
  printf("Press Ctrl+C to abort current expression, Ctrl+D to exit the "
         "REPL\n");
  printf("%s.help%s     show this help\n", v6_c_bold(color), v6_c_reset(color));
  printf("%s.exit%s     exit the REPL\n", v6_c_bold(color), v6_c_reset(color));
  printf("%s.break%s    cancel the current multi-line input\n",
         v6_c_bold(color), v6_c_reset(color));
  printf("%s.clear%s    reset the REPL session\n", v6_c_bold(color),
         v6_c_reset(color));
  printf("%s.history%s  print input history\n", v6_c_bold(color),
         v6_c_reset(color));
  printf("%s.save <file>%s   save session source to a file\n", v6_c_bold(color),
         v6_c_reset(color));
  printf("%s.load <file>%s   load and run a file in this session\n",
         v6_c_bold(color), v6_c_reset(color));
}

int v6_repl_run(v6_cli_options* opts) {
  v6_color_init(opts->color_mode);
  int color = v6_color_enabled_out();

  printf("v6 %s -- type .help for REPL commands\n", V6_VERSION);

  repl_history hist;
  history_load(&hist);

  char* accumulated = malloc(1);
  accumulated[0] = '\0';
  size_t accumulated_cap = 1;
  size_t prev_output_len = 0;

  char pending[1 << 16];
  pending[0] = '\0';

  int interactive = stdin_is_tty();
  if (interactive)
    enable_raw_mode();

  for (;;) {
    const char* prompt = pending[0] ? "... " : "> ";
    char line[V6_REPL_LINE_CAP];
    read_status st =
        interactive ? read_line_interactive(prompt, color, &hist, line,
                                            sizeof(line), pending[0] == '\0')
                    : read_line_piped(prompt, color, line, sizeof(line));
    if (st == read_eof || st == read_exit_requested)
      break;
    if (st == read_interrupted) {
      pending[0] = '\0';
      continue;
    }

    if (pending[0] == '\0') {
      if (strcmp(line, ".exit") == 0)
        break;
      if (strcmp(line, ".help") == 0) {
        print_help(color);
        continue;
      }
      if (strcmp(line, ".break") == 0) {
        pending[0] = '\0';
        continue;
      }
      if (strcmp(line, ".clear") == 0) {
        accumulated[0] = '\0';
        prev_output_len = 0;
        printf("session cleared\n");
        continue;
      }
      if (strcmp(line, ".history") == 0) {
        for (int i = 0; i < hist.count; i++)
          printf("%s\n", hist.items[i]);
        continue;
      }
      if (strncmp(line, ".save ", 6) == 0) {
        FILE* f = fopen(line + 6, "wb");
        if (f) {
          fwrite(accumulated, 1, strlen(accumulated), f);
          fclose(f);
          printf("saved to %s\n", line + 6);
        } else {
          printf("error: cannot write %s\n", line + 6);
        }
        continue;
      }
      if (strncmp(line, ".load ", 6) == 0) {
        FILE* f = fopen(line + 6, "rb");
        if (!f) {
          printf("error: cannot read %s\n", line + 6);
          continue;
        }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* contents = malloc((size_t)n + 1);
        fread(contents, 1, (size_t)n, f);
        contents[n] = '\0';
        fclose(f);
        strncpy(pending, contents, sizeof(pending) - 1);
        pending[sizeof(pending) - 1] = '\0';
        free(contents);
        line[0] = '\0';
      }
    }

    if (line[0] != '\0' || pending[0] != '\0') {
      if (line[0] != '\0') {
        if (pending[0])
          strncat(pending, "\n", sizeof(pending) - strlen(pending) - 1);
        strncat(pending, line, sizeof(pending) - strlen(pending) - 1);
      }
    }

    if (pending[0] == '\0')
      continue;

    if (!input_is_balanced(pending))
      continue;

    history_add(&hist, pending);

    char wrapped[1 << 16];
    wrap_line(pending, wrapped, sizeof(wrapped));
    pending[0] = '\0';

    size_t need = strlen(accumulated) + strlen(wrapped) + 2;
    if (need > accumulated_cap) {
      accumulated_cap = need * 2;
      accumulated = realloc(accumulated, accumulated_cap);
    }
    char* tentative = malloc(strlen(accumulated) + strlen(wrapped) + 2);
    sprintf(tentative, "%s%s\n", accumulated, wrapped);

    buf out_bytes;
    module_ctx modctx;
    char err_msg[1024];
    int err_line = 0;
    if (!try_compile(tentative, "[repl]", &out_bytes, &modctx, err_msg,
                     sizeof(err_msg), &err_line)) {
      printf("%s%sSyntaxError%s: %s\n", v6_c_bold(color), v6_c_red(color),
             v6_c_reset(color), err_msg);
      free(tentative);
      continue;
    }

    int exit_code = 1;
    size_t total_len = 0;
    char* captured =
        capture_and_run(opts, &out_bytes, &modctx, &exit_code, &total_len);
    buf_free(&out_bytes);

    if (captured) {
      size_t new_len =
          total_len > prev_output_len ? total_len - prev_output_len : 0;
      fwrite(captured + (total_len - new_len), 1, new_len, stdout);
      fflush(stdout);
      if (exit_code == 0) {
        free(accumulated);
        accumulated = tentative;
        accumulated_cap = strlen(tentative) + 1;
        prev_output_len = total_len;
        tentative = NULL;
      }
      free(captured);
    }
    free(tentative);
  }

  disable_raw_mode();
  history_save(&hist);
  for (int i = 0; i < hist.count; i++)
    free(hist.items[i]);
  free(accumulated);
  return 0;
}

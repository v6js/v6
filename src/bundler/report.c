#include "v6/bundler_report.h"
#include "v6/color.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define v6_strnicmp _strnicmp
#else
#include <strings.h>
#define v6_strnicmp strncasecmp
#endif

void v6_bundler_clear_screen(void) {
#ifdef _WIN32
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO info;
  if (h == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(h, &info)) {
    printf("\x1b[2J\x1b[H");
    return;
  }
  DWORD cell_count = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
  COORD origin = {0, 0};
  DWORD written;
  FillConsoleOutputCharacterA(h, ' ', cell_count, origin, &written);
  FillConsoleOutputAttribute(h, info.wAttributes, cell_count, origin, &written);
  SetConsoleCursorPosition(h, origin);
#else
  printf("\x1b[2J\x1b[H");
#endif
  fflush(stdout);
}

void v6_bundler_format_size(double bytes, char* out, size_t out_size) {
  static const char* units[] = {"B", "kB", "MB", "GB"};
  int u = 0;
  double v = bytes;
  while (v >= 1024.0 && u < 3) {
    v /= 1024.0;
    u++;
  }
  if (u == 0) {
    snprintf(out, out_size, "%d B", (int)bytes);
  } else {
    snprintf(out, out_size, "%.2f %s", v, units[u]);
  }
}

int v6_bundler_parse_size(const char* s, long long* out_bytes) {
  char* end;
  double v = strtod(s, &end);
  if (end == s)
    return -1;
  while (*end == ' ')
    end++;

  long long mult = 1;
  if (v6_strnicmp(end, "kb", 2) == 0)
    mult = 1024;
  else if (v6_strnicmp(end, "mb", 2) == 0)
    mult = 1024LL * 1024;
  else if (v6_strnicmp(end, "gb", 2) == 0)
    mult = 1024LL * 1024 * 1024;
  else if (v6_strnicmp(end, "k", 1) == 0)
    mult = 1024;
  else if (v6_strnicmp(end, "m", 1) == 0)
    mult = 1024LL * 1024;
  else if (v6_strnicmp(end, "g", 1) == 0)
    mult = 1024LL * 1024 * 1024;
  else if (*end != '\0' && v6_strnicmp(end, "b", 1) != 0)
    return -1;

  *out_bytes = (long long)(v * (double)mult);
  return 0;
}

void v6_bundler_print_bundle_summary(const v6_bundler_graph* g,
                                     const char* outfile, size_t out_len,
                                     v6_bundler_verbosity verbosity) {
  if (verbosity == v6_bundler_verbosity_quiet)
    return;

  int c = v6_color_enabled_out();
  char size_buf[32];

  printf("%s%s%10s%s %s%s%s\n", v6_c_bold(c), v6_c_green(c), "Bundled",
         v6_c_reset(c), v6_c_bold(c), outfile, v6_c_reset(c));

  int name_width = 0;
  for (int i = 0; i < g->count; i++) {
    int len = (int)strlen(g->modules[i]->id);
    if (len > name_width)
      name_width = len;
  }
  if (name_width > 60)
    name_width = 60;
  if (name_width < 5)
    name_width = 5;

  for (int i = 0; i < g->count; i++) {
    v6_bundler_format_size((double)g->modules[i]->source_len, size_buf,
                           sizeof(size_buf));
    printf("           %s%-*s%s  %s%8s%s\n", v6_c_dim(c), name_width,
           g->modules[i]->id, v6_c_reset(c), v6_c_cyan(c), size_buf,
           v6_c_reset(c));

    if (verbosity == v6_bundler_verbosity_verbose) {
      for (int j = 0; j < g->modules[i]->import_count; j++) {
        v6_bundler_import_edge* e = &g->modules[i]->imports[j];
        printf("             %s%s %s -> %s%s\n", v6_c_dim(c), "resolves",
               e->specifier, e->target ? e->target->id : "(external)",
               v6_c_reset(c));
      }
    }
  }

  v6_bundler_format_size((double)out_len, size_buf, sizeof(size_buf));
  printf("%s%10s%s %s%-*s%s  %s%8s%s  %s(%d module%s)%s\n", v6_c_bold(c),
         "Total", v6_c_reset(c), v6_c_bold(c), name_width, "", v6_c_reset(c),
         v6_c_bold(c), size_buf, v6_c_reset(c), v6_c_dim(c), g->count,
         g->count == 1 ? "" : "s", v6_c_reset(c));
}

int v6_bundler_check_limits(const v6_bundler_graph* g, size_t out_len,
                            const v6_bundler_limits* limits,
                            const char* outfile) {
  int c = v6_color_enabled_err();
  int failed = 0;
  char size_buf[32];
  char limit_buf[32];

  if (limits->max_size > 0 && (long long)out_len > limits->max_size) {
    v6_bundler_format_size((double)out_len, size_buf, sizeof(size_buf));
    v6_bundler_format_size((double)limits->max_size, limit_buf,
                           sizeof(limit_buf));
    int is_error = limits->mode == v6_bundler_limit_error;
    fprintf(stderr, "%s%s%s%s: %s exceeds the size limit (%s > %s)\n",
            v6_c_bold(c), is_error ? v6_c_red(c) : v6_c_yellow(c),
            is_error ? "error" : "warning", v6_c_reset(c), outfile, size_buf,
            limit_buf);
    if (is_error)
      failed = 1;
  }

  if (limits->max_deps > 0 && (long long)g->count > limits->max_deps) {
    int is_error = limits->mode == v6_bundler_limit_error;
    fprintf(stderr,
            "%s%s%s%s: %d modules exceeds the dependency limit (%lld)\n",
            v6_c_bold(c), is_error ? v6_c_red(c) : v6_c_yellow(c),
            is_error ? "error" : "warning", v6_c_reset(c), g->count,
            limits->max_deps);
    if (is_error)
      failed = 1;
  }

  return failed;
}

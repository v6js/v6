#include "v6/color.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define v6_isatty(fd) _isatty(fd)
#define v6_fileno(f) _fileno(f)
#else
#include <unistd.h>
#define v6_isatty(fd) isatty(fd)
#define v6_fileno(f) fileno(f)
#endif

static int g_out_enabled = 0;
static int g_err_enabled = 0;

#ifdef _WIN32
static void enable_vt_mode(DWORD std_handle) {
  HANDLE h = GetStdHandle(std_handle);
  if (h == INVALID_HANDLE_VALUE)
    return;
  DWORD mode = 0;
  if (!GetConsoleMode(h, &mode))
    return;
  SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#endif

static int stream_is_tty(FILE* f) {
  return v6_isatty(v6_fileno(f)) != 0;
}

void v6_color_init(v6_color_mode mode) {
  if (mode == v6_color_never) {
    g_out_enabled = 0;
    g_err_enabled = 0;
    return;
  }

  const char* no_color = getenv("NO_COLOR");
  int forced_off = no_color && no_color[0] != '\0' && mode == v6_color_auto;

  if (forced_off) {
    g_out_enabled = 0;
    g_err_enabled = 0;
    return;
  }

  if (mode == v6_color_always) {
    g_out_enabled = 1;
    g_err_enabled = 1;
  } else {
    g_out_enabled = stream_is_tty(stdout);
    g_err_enabled = stream_is_tty(stderr);
  }

#ifdef _WIN32
  if (g_out_enabled)
    enable_vt_mode(STD_OUTPUT_HANDLE);
  if (g_err_enabled)
    enable_vt_mode(STD_ERROR_HANDLE);
#endif
}

int v6_color_enabled_out(void) {
  return g_out_enabled;
}

int v6_color_enabled_err(void) {
  return g_err_enabled;
}

const char* v6_c_reset(int enabled) {
  return enabled ? "\x1b[0m" : "";
}

const char* v6_c_bold(int enabled) {
  return enabled ? "\x1b[1m" : "";
}

const char* v6_c_dim(int enabled) {
  return enabled ? "\x1b[2m" : "";
}

const char* v6_c_red(int enabled) {
  return enabled ? "\x1b[31m" : "";
}

const char* v6_c_green(int enabled) {
  return enabled ? "\x1b[32m" : "";
}

const char* v6_c_yellow(int enabled) {
  return enabled ? "\x1b[33m" : "";
}

const char* v6_c_cyan(int enabled) {
  return enabled ? "\x1b[36m" : "";
}

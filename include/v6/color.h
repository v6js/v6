#pragma once

typedef enum {
  v6_color_auto = 0,
  v6_color_always = 1,
  v6_color_never = 2,
} v6_color_mode;

void v6_color_init(v6_color_mode mode);
int v6_color_enabled_out(void);
int v6_color_enabled_err(void);

const char* v6_c_reset(int enabled);
const char* v6_c_bold(int enabled);
const char* v6_c_dim(int enabled);
const char* v6_c_red(int enabled);
const char* v6_c_green(int enabled);
const char* v6_c_yellow(int enabled);
const char* v6_c_cyan(int enabled);

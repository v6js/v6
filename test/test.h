#pragma once

#include <stdio.h>

#define v6_check(fails, cond)                                                  \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "fail: %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
      (*(fails))++;                                                            \
    }                                                                          \
  } while (0)

#include "v6/jvm.h"

#include <stdio.h>

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  printf("v6 0.0.1\n");
  printf("jni: %s\n", v6_jvm_available() ? "yes" : "no");

  return 0;
}

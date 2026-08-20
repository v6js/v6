static int addSelf(int x) {
  return x + x;
}

static int addOne(int x) {
  return x + 1;
}

typedef int (*fnptr)(int);
static fnptr table[2] = {addSelf, addOne};
static volatile int idx = 1;

__attribute__((export_name("run")))
int run(void) {
  fnptr f = table[idx];
  return f(5);
}

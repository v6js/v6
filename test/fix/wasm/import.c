__attribute__((import_module("env"), import_name("dbl")))
extern int dbl_import(int x);

__attribute__((export_name("dbl")))
int dbl(int x) {
  return dbl_import(x);
}

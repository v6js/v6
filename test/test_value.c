#include "test.h"
#include "v6/value.h"

#include <math.h>

int test_value(void) {
  int fails = 0;

  v6_check(&fails, v6_is_num(v6_num(1.5)));
  v6_check(&fails, v6_as_num(v6_num(1.5)) == 1.5);
  v6_check(&fails, v6_is_num(v6_num(0.0)));
  v6_check(&fails, v6_is_num(v6_num(-0.0)));
  v6_check(&fails, v6_is_num(v6_num(NAN)));
  v6_check(&fails, !v6_is_num(v6_null()));
  v6_check(&fails, !v6_is_num(v6_undef()));
  v6_check(&fails, !v6_is_num(v6_bool(1)));

  v6_check(&fails, v6_is_null(v6_null()));
  v6_check(&fails, !v6_is_null(v6_undef()));

  v6_check(&fails, v6_is_undef(v6_undef()));
  v6_check(&fails, !v6_is_undef(v6_null()));

  v6_check(&fails, v6_is_bool(v6_bool(0)));
  v6_check(&fails, v6_is_bool(v6_bool(1)));
  v6_check(&fails, v6_as_bool(v6_bool(1)) == 1);
  v6_check(&fails, v6_as_bool(v6_bool(0)) == 0);
  v6_check(&fails, !v6_is_bool(v6_null()));

  int x = 42;
  v6_val v = v6_obj(&x);
  v6_check(&fails, v6_is_obj(v));
  v6_check(&fails, v6_as_obj(v) == &x);
  v6_check(&fails, !v6_is_obj(v6_num(1.0)));

  return fails;
}

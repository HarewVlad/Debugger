#include "target_include.h"

void Additional(int c) {
  int a = c * 10 / 20;
}

void Method(int a, int b, float c) {
  int d = 20;
  int e = 33;
  Additional(c);
  int f = 10 + d * e;
}
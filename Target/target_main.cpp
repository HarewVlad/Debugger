#include "target_include.h"
#include <Windows.h>
#include <iostream>

void Function() {
  int c = 30;
  int a = 20 + c * 123;
}

template <typename T>
int Bar(T a, T b) {
  return a + b;
}

int main(int argc, char **argv) {
  // OutputDebugStringA("HELLO DUDE!");
  Function();
  Method(10, 20, 3.5f);

  {
    int a = Bar<int>(10, 10);
    int b = a;
  }

  for (int i = 0; i < 5; ++i) {
    std::cout << "Hello";
  }

  return 1;
}
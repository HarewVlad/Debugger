#include "target_include.h"
#include <Windows.h>
#include <iostream>

void Function() {
  int c = 30;
  int a = 20 + c * 123;
}

int main(int argc, char **argv) {
  // OutputDebugStringA("HELLO DUDE!");
  Function();
  Method(10, 20, 3.5f);

  for (int i = 0; i < 5; ++i) {
    std::cout << "Hello";
  }

  return 1;
}
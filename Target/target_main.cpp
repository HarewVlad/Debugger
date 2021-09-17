#include <Windows.h>
#include "target_include.h"

void Function() {
	int c = 30;
	int a = 20 + c * 123;
}

int main() {
	// OutputDebugStringA("HELLO DUDE!");
	Function();
	Method();

	return 1;
}
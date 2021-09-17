#include <Windows.h>

void Function() {
	int c = 30;
	int a = 20 + c * 123;
}

int main() {
	OutputDebugStringA("HELLO DUDE!");
	Function();

	return 1;
}
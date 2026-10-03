#include <conio.h>
#include <iostream>
using namespace std;

int main() {
	cout << "conio demo" << endl;
	cout << "type text, EXE new line, EXIT quit" << endl;
	cout << "> ";
	for (;;) {
		if (!_kbhit()) continue;
		int c = _getch();
		if (c == 27) break;
		if (c == 13) { cout << endl << "> "; continue; }
		if (c == 8) { cout << '\b' << ' ' << '\b'; continue; }
		if (c == 0) continue;
		if (c >= 0x20 && c < 0x7F) cout << (char)c;
	}
	cout << endl << "done" << endl;
	return 0;
}

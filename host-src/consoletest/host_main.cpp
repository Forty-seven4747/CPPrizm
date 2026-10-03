#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include <fxcg/display.h>
#include <fxcg/keyboard.h>

#include <iostream>
#include <conio.h>
using namespace std;

extern "C" void prizm_con_end(void);

/* Same configuration macros the console uses, so the fake screen knows which
   font scheme it is modelling. The defaults match prizm_console.cpp, pass
   -DPRIZM_CON_FONT=n on the command line to test another one. */
#ifndef PRIZM_CON_FONT
#define PRIZM_CON_FONT 2
#endif
#ifndef PRIZM_CON_BG
#define PRIZM_CON_BG 0
#endif

#include "prizm_console_layout.h"

#define H_COLS    CON_COLS
#define H_ROWS    CON_ROWS
#define H_ROW_TOP ROW_TOP
#define H_ROW_H   ROW_H
#define H_CELL_W  COL_W

static unsigned short g_vram[384 * 216];
static char g_screen[H_ROWS][64];

/* What the OS would actually put on the glass, as TEXT_COLOR_* indices, so the
   colour of the background and of the glyphs can be asserted instead of being
   eyeballed.  -1 means "this call leaves that alone", which is correct when
   the console has already filled the strip with the paper colour. */
static signed char g_bgIdx[H_ROWS][64];
static signed char g_fgIdx[H_ROWS][64];

static signed char rgb2idx(unsigned short v) {
	if (v == COLOR_BLACK) return TEXT_COLOR_BLACK;
	if (v == COLOR_WHITE) return TEXT_COLOR_WHITE;
	return -1;
}

void* GetVRAMAddress(void) { return g_vram; }
void Bdisp_EnableColor(int) {}
void Bdisp_PutDisp_DD(void) {}
void Bdisp_PutDisp_DD_stripe(int, int) {}

void PrintXY(int x, int y, const char* string, int mode, int color) {
	int row = y - 1;
	if (row < 0 || row >= H_ROWS) return;
	const char* s = string + 2;
	int col = x - 1;
	for (int i = 0; s[i] && col + i < 63; i++) {
		g_screen[row][col + i] = s[i];
		/* 0x20 is the transparent background mode the console uses. */
		g_bgIdx[row][col + i] = (mode & 0x20) ? -1 : TEXT_COLOR_WHITE;
		g_fgIdx[row][col + i] = (signed char)color;
	}
}

/* The real console draws one character per fixed cell with a variable width
   font, so the fake only has to turn the pixel coordinates back into a cell.
   writeflag 0 means measure only. */
void PrintMini(int* x, int* y, const char* string, int mode_flags, unsigned int xlimit,
               int P6, int P7, int color, int back_color, int writeflag, int P11) {
	(void)mode_flags; (void)xlimit; (void)P6; (void)P7; (void)P11;
	if (writeflag && string && string[0]) {
		int col = *x / H_CELL_W;
		int row = (*y - H_ROW_TOP) / H_ROW_H;
		if (row >= 0 && row < H_ROWS && col >= 0 && col < 63) {
			g_screen[row][col] = string[0];
			g_bgIdx[row][col] = rgb2idx((unsigned short)back_color);
			g_fgIdx[row][col] = rgb2idx((unsigned short)color);
		}
	}
	*x += H_CELL_W;
}

/* Here simulate != 0 means "do not draw", the opposite of writeflag above.
   The mode bits are modelled the way the hardware behaves: the background
   defaults to white and is painted behind the glyph unless bit 7 says to
   leave it alone, bit 0 makes the color argument the back colour with a white
   font, and bit 2 writes a black back colour with the font inverted.  This is
   exactly the default that made the small font paint white boxes on a black
   paper, so it is worth being able to assert on. */
void PrintMiniMini(int* x, int* y, const char* string, int mode1, char color, int mode2) {
	if (mode2 == 0 && string && string[0]) {
		int col = *x / H_CELL_W;
		int row = (*y - H_ROW_TOP) / H_ROW_H;
		if (row >= 0 && row < H_ROWS && col >= 0 && col < 63) {
			int back = TEXT_COLOR_WHITE;
			int font = (unsigned char)color;
			if (mode1 & 0x01) { back = (unsigned char)color; font = TEXT_COLOR_WHITE; }
			if (mode1 & 0x04) { back = TEXT_COLOR_BLACK;  font = TEXT_COLOR_WHITE; }
			if (mode1 & 0x80) { back = -1; }
			g_screen[row][col] = string[0];
			g_bgIdx[row][col] = (signed char)back;
			g_fgIdx[row][col] = (signed char)font;
		}
	}
	*x += H_CELL_W;
}

struct FakeEvent { int key; int mat; };
static FakeEvent g_ev[512];
static int g_evN = 0, g_evI = 0;

extern "C" void testPushKey(int keycode) {
	if (g_evN < 512) { g_ev[g_evN].key = keycode; g_ev[g_evN].mat = 0; g_evN++; }
}

extern "C" void testPushMat(int matrix) {
	if (g_evN < 512) { g_ev[g_evN].key = 0; g_ev[g_evN].mat = matrix; g_evN++; }
}

extern "C" void testClearKeys(void) { g_evN = 0; g_evI = 0; }

int GetKey(int* key) {
	if (g_evI < g_evN) { *key = g_ev[g_evI].key; g_evI++; return 0; }
	*key = KEY_CTRL_EXIT;
	return 0;
}

int GetKeyWait_OS(int* column, int* row, int type_of_waiting,
                  int timeout_period, int menu, unsigned short* keycode) {
	(void)timeout_period; (void)menu; (void)keycode;
	if (g_evI >= g_evN) {
		if (type_of_waiting == KEYWAIT_HALTOFF_TIMEROFF) return KEYREP_NOEVENT;
		*column = 4; *row = 8;
		return KEYREP_KEYEVENT;
	}
	int m = g_ev[g_evI].mat;
	*column = (m >> 8) & 0xFF;
	*row = m & 0xFF;
	return KEYREP_KEYEVENT;
}

static void resetScreen() {
	for (int y = 0; y < H_ROWS; y++) {
		memset(g_screen[y], ' ', 63);
		g_screen[y][63] = 0;
		memset(g_bgIdx[y], -1, 64);
		memset(g_fgIdx[y], -1, 64);
	}
	/* g_vram is deliberately not reset: the console fills it once, on its
	   first draw, and the colour probe below samples what it left there. */
}

static void outRaw(const char* s) { fputs(s, stdout); }

static void outFmt(const char* fmt, ...) {
	char buf[512];
	va_list ap; va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > 0) outRaw(buf);
}

static void dumpScreen(const char* title) {
	outFmt("\n%s\n", title);
	for (int y = 0; y < H_ROWS; y++) {
		char line[64];
		memcpy(line, g_screen[y], 64);
		int n = 63;
		while (n > 0 && line[n - 1] == ' ') n--;
		line[n] = 0;
		outFmt("  row%d |%s|\n", y, line);
	}
	fflush(stdout);
}

static int g_pass = 0, g_fail = 0;

static void ck(const char* what, int got, int want) {
	if (got == want) { g_pass++; return; }
	g_fail++;
	outFmt("  [FAIL] %-34s got=%-8d want=%-8d\n", what, got, want);
}

/* Regression probe for the bug that shipped in font_demo_small_black: the 10 px
   font syscall takes no back_color, so whatever the OS paints behind a glyph
   has to come from the mode bits, and its default is white.  On a black paper
   that painted a white box behind every character.  The fakes above record
   what the hardware would have put on the glass, so this fails on the old code
   and passes on the fixed one, without needing a calculator. */
static void probeColours(const char* tag) {
	const int paper = PRIZM_CON_BG ? TEXT_COLOR_BLACK : TEXT_COLOR_WHITE;
	const int ink   = PRIZM_CON_BG ? TEXT_COLOR_WHITE : TEXT_COLOR_BLACK;
	const unsigned short paperRGB = (unsigned short)(PRIZM_CON_BG ? COLOR_BLACK : COLOR_WHITE);
	int drawn = 0, wrongBg = 0, wrongInk = 0;

	for (int r = 0; r < H_ROWS; r++)
		for (int c = 0; c < H_COLS; c++) {
			if (g_bgIdx[r][c] < 0 && g_fgIdx[r][c] < 0) continue;
			drawn++;
			if (g_bgIdx[r][c] >= 0 && g_bgIdx[r][c] != paper) wrongBg++;
			if (g_fgIdx[r][c] >= 0 && g_fgIdx[r][c] != ink)   wrongInk++;
		}

	char label[80];
	snprintf(label, sizeof(label), "%s cells drawn", tag);
	ck(label, drawn > 0, 1);
	snprintf(label, sizeof(label), "%s background=paper", tag);
	ck(label, wrongBg, 0);
	snprintf(label, sizeof(label), "%s glyph=ink", tag);
	ck(label, wrongInk, 0);
	/* Whatever the draw calls leave alone has to already be the paper colour. */
	snprintf(label, sizeof(label), "%s own fill", tag);
	ck(label, g_vram[(H_ROW_TOP + 2) * 384 + 5] == paperRGB, 1);
}

int main() {
	resetScreen();

	outRaw("console: iostream / printf\n");

	cout << "Hello world!" << endl;
	dumpScreen("A. cout << \"Hello world!\" << endl");

	cout << 42 << " " << -7 << " " << 3.14159 << endl;
	dumpScreen("B. int / negative / double");

	cout << "ABCDEFGHIJKLMNOPQRSTUVWXYZ" << endl;
	dumpScreen("C. wrap at the right edge");
	probeColours("colour");

	for (int i = 1; i <= 10; i++) cout << "line " << i << endl;
	dumpScreen("D. scroll up");

	cout << "abc" << '\b' << "XYZ";
	dumpScreen("E. backspace abc\\bXYZ");

	printf("F1 [%5d][%-5d][%05d][%+d]\n", 42, 42, 42, 42);
	printf("F2 [%.3f][%8.2f][%s][%.2s]\n", 3.14159, 3.14159, "abcdef", "abcdef");
	printf("F3 [%ld][%lld][%u][%o][%c][%%]\n", 123456L, 1234567890123LL, 7u, 64, 'Z');
	dumpScreen("F. printf width / precision / flags");

	{
		char sb[80];
		sprintf(sb, "sprintf[%5.2f][%s][%03d]", 2.5, "ok", 7);
		outFmt("\n  sprintf -> |%s|\n", sb);
		fflush(stdout);
	}

	testClearKeys();
	cout << "n = ";
	testPushKey('1'); testPushKey('2'); testPushKey('3'); testPushKey(KEY_CTRL_EXE);
	int n = 0;
	if (cin >> n) cout << "got " << n << " x2=" << (n * 2) << endl;
	else          cout << "cancelled" << endl;
	dumpScreen("G. cin >> int");

	testClearKeys();
	cout << "a b = ";
	testPushKey('7'); testPushKey(' '); testPushKey('-'); testPushKey('8'); testPushKey(KEY_CTRL_EXE);
	int a = 0, b = 0;
	cin >> a >> b;
	cout << "a=" << a << " b=" << b << endl;
	dumpScreen("H. cin >> a >> b");

	testClearKeys();
	cout << "cancel test: ";
	testPushKey(KEY_CTRL_AC);
	int z = 5;
	if (cin >> z) cout << "ok " << z << endl;
	else          cout << "fail() = " << cin.fail() << endl;
	dumpScreen("I. cin cancelled");

	cout << "bye";
	prizm_con_end();
	dumpScreen("J. after prizm_con_end()");
	probeColours("prompt");

	outRaw("\nconio: _getch / _kbhit / _getwch\n");

	testClearKeys();
	ck("no key: _kbhit()==0", _kbhit(), 0);

	testClearKeys();
	testPushKey(KEY_CHAR_A);
	ck("letter: _kbhit()==1", _kbhit(), 1);
	ck("letter: _getch()=='A'", _getch(), 'A');
	ck("after read: _kbhit()==0", _kbhit(), 0);

	testClearKeys();
	testPushKey(KEY_CHAR_7); testPushKey(KEY_CHAR_PLUS); testPushKey(KEY_CHAR_1);
	ck("digit '7'", _getch(), '7');
	ck("plus '+'", _getch(), '+');
	ck("digit '1'", _getch(), '1');

	testClearKeys();
	testPushKey(KEY_CHAR_MINUS); testPushKey(KEY_CHAR_MULT); testPushKey(KEY_CHAR_DIV);
	testPushKey(KEY_CHAR_POW); testPushKey(KEY_CHAR_PMINUS); testPushKey(KEY_CHAR_DP);
	ck("minus '-'", _getch(), '-');
	ck("mult '*'", _getch(), '*');
	ck("div '/'", _getch(), '/');
	ck("pow '^'", _getch(), '^');
	ck("pm'ins '-'", _getch(), '-');
	ck("dot '.'", _getch(), '.');

	testClearKeys();
	testPushKey(KEY_CHAR_FRAC); testPushKey(KEY_CHAR_EXP);
	ck("frac '/'", _getch(), '/');
	ck("exp 'E'", _getch(), 'E');

	testClearKeys();
	testPushKey(KEY_CHAR_LPAR); testPushKey(KEY_CHAR_RPAR); testPushKey(KEY_CHAR_COMMA);
	ck("lpar '('", _getch(), '(');
	ck("rpar ')'", _getch(), ')');
	ck("comma ','", _getch(), ',');

	testClearKeys();
	testPushKey(KEY_CTRL_EXE);
	ck("EXE -> 13", _getch(), 13);
	testClearKeys();
	testPushKey(KEY_CTRL_DEL);
	ck("DEL -> 8", _getch(), 8);
	testClearKeys();
	testPushKey(KEY_CTRL_EXIT);
	ck("EXIT -> 27", _getch(), 27);
	testClearKeys();
	testPushKey(KEY_CTRL_AC);
	ck("AC -> 3", _getch(), 3);

	testClearKeys();
	testPushKey(KEY_CTRL_SHIFT); testPushKey(KEY_CTRL_ALPHA); testPushKey(KEY_CHAR_Z);
	ck("SHIFT/ALPHA skipped", _getch(), 'Z');

	testClearKeys();
	testPushKey(KEY_CHAR_SIN); testPushKey(KEY_CHAR_ANGLE); testPushKey(KEY_CHAR_7);
	ck("non-ascii keys skipped", _getch(), '7');

	testClearKeys();
	testPushKey(KEY_CTRL_UP);
	ck("UP: _kbhit()==1", _kbhit(), 1);
	ck("UP: first call -> 0", _getch(), 0);
	ck("UP: second call -> 72", _getch(), 72);

	testClearKeys();
	testPushKey(KEY_CTRL_LEFT); testPushKey(KEY_CTRL_DOWN);
	testPushKey(KEY_CTRL_RIGHT); testPushKey(KEY_CTRL_F1);
	ck("LEFT -> 75", (_getch() == 0) ? _getch() : -1, 75);
	ck("DOWN -> 80", (_getch() == 0) ? _getch() : -1, 80);
	ck("RIGHT -> 77", (_getch() == 0) ? _getch() : -1, 77);
	ck("F1 -> 59", (_getch() == 0) ? _getch() : -1, 59);

	testClearKeys();
	testPushKey(KEY_CTRL_UP);
	ck("_getwch prefix 0xE0", _getwch(), 0xE0);
	ck("_getwch code 72", _getwch(), 72);

	testClearKeys();
	ck("_ungetch returns", _ungetch('Q'), 'Q');
	ck("_kbhit sees pushback", _kbhit(), 1);
	ck("pop pushback", _getch(), 'Q');

	testClearKeys();
	testPushKey(KEY_CTRL_F6);
	ck("F6 -> 64", (_getch() == 0) ? _getch() : -1, 64);

	ck("key_extend UP", prizm_key_extend(KEY_CTRL_UP), 72);
	ck("key_extend F6", prizm_key_extend(KEY_CTRL_F6), 64);
	ck("key_extend 'A'", prizm_key_extend(KEY_CHAR_A), 0);

	testClearKeys();
	testPushKey(KEY_CHAR_B);
	ck("prizm_getkey returns code", prizm_getkey(), KEY_CHAR_B);

	testClearKeys();
	testPushMat(0x0608);
	{
		int mm = 0;
		prizm_getraw(&mm);
		ck("prizm_getraw matrix", mm, 0x0608);
	}

	testClearKeys();
	ck("keypoll empty -> 0", prizm_keypoll(0), 0);
	testPushMat(0x0703);
	{
		int mm = 0;
		ck("keypoll event -> 1", prizm_keypoll(&mm), 1);
		ck("keypoll matrix", mm, 0x0703);
	}

	resetScreen();
	_putch('A');
	_cputs("BC");
	_cprintf("[%d|%s]", 42, "xy");
	_putwch('D');
	_putws(L"EF");
	dumpScreen("K. _putch / _cputs / _cprintf / _putwch / _putws");

	outFmt("\npass=%d fail=%d\n", g_pass, g_fail);
	outRaw("all console tests done\n");
	fflush(stdout);
	return g_fail ? 1 : 0;
}

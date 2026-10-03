#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include <fxcg/display.h>
#include <fxcg/keyboard.h>

#include <iostream>
#include <conio.h>

/* ---------------------------------------------------------------------------
   Console appearance

   Both choices are normally written by cpprizm-gui.exe into
   src\prizm_console_cfg.h, so one prizm_console.cpp serves every combination.
   Compiling the file on its own, or with the PC test harness, simply falls
   back to the defaults below.

     PRIZM_CON_FONT   1 = big, 2 = medium (default), 3 = small
     PRIZM_CON_BG     0 = white paper with dark ink (default), 1 = the reverse

   The geometry of each font scheme lives in prizm_console_layout.h.
   --------------------------------------------------------------------------- */
#if defined(__has_include)
#  if __has_include("prizm_console_cfg.h")
#    include "prizm_console_cfg.h"
#  endif
#endif

#ifndef PRIZM_CON_FONT
#define PRIZM_CON_FONT 2
#endif
#ifndef PRIZM_CON_BG
#define PRIZM_CON_BG 0
#endif

#include "prizm_console_layout.h"

/* TEXT_COLOR_* index 0 means "ink" and 7 means "paper", so the console keeps
   its own colour table instead of hard coding black and white.  On a black
   paper the two simply swap, and textcolor() keeps working either way. */
#define CON_PAPER ((color_t)(PRIZM_CON_BG ? COLOR_BLACK : COLOR_WHITE))
#define CON_INK   ((color_t)(PRIZM_CON_BG ? COLOR_WHITE : COLOR_BLACK))

static char g_scr[CON_VROWS][CON_COLS];
static int  g_col = 0;
static int  g_row = 0;
static int  g_inited = 0;
static int  g_used = 0;
static int  g_fg = TEXT_COLOR_BLACK;
static int  g_cursorVisible = 0;
static unsigned short* g_vram = 0;

#if PRIZM_CON_FONT != 1
/* PrintMini takes a real RGB565 colour, so the palette is needed there only. */
static const unsigned short CON_PAL[8] = {
	(unsigned short)CON_INK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
	COLOR_RED, COLOR_PURPLE, COLOR_YELLOW, (unsigned short)CON_PAPER
};
#endif

/* PrintXY and PrintMiniMini take an OS palette index instead, where 0 is black
   and 7 is white.  Map the console's ink and paper onto the right end.
   PrintXY always needs this, and PrintMiniMini needs it on white paper.  On a
   black paper that font takes its back colour from a mode bit instead, so the
   palette index no longer selects the glyph colour there. */
#if PRIZM_CON_FONT == 1 || (PRIZM_CON_FONT == 3 && PRIZM_CON_BG == 0)
static int conIdx(int i) {
	i &= 7;
	if (i == TEXT_COLOR_BLACK) return PRIZM_CON_BG ? TEXT_COLOR_WHITE : TEXT_COLOR_BLACK;
	if (i == TEXT_COLOR_WHITE) return PRIZM_CON_BG ? TEXT_COLOR_BLACK : TEXT_COLOR_WHITE;
	return i;
}
#endif

static void hwInit(void) {
	Bdisp_EnableColor(1);
	g_vram = (unsigned short*)GetVRAMAddress();
	int n = LCD_WIDTH_PX * LCD_HEIGHT_PX;
	for (int i = 0; i < n; i++) g_vram[i] = CON_PAPER;
	Bdisp_PutDisp_DD();
}

/* Fill a pixel rectangle with the paper colour. */
static void hwFillRect(int x0, int y0, int w, int h, unsigned short col) {
	if (!g_vram) g_vram = (unsigned short*)GetVRAMAddress();
	for (int y = y0; y < y0 + h; y++) {
		if (y < 0 || y >= LCD_HEIGHT_PX) continue;
		unsigned short* p = g_vram + y * LCD_WIDTH_PX;
		for (int x = x0; x < x0 + w; x++) {
			if (x < 0 || x >= LCD_WIDTH_PX) continue;
			p[x] = col;
		}
	}
}

/* Fill whole pixel lines with the paper colour. */
static void hwClearBand(int y0, int y1) {
	if (y0 < 0) y0 = 0;
	if (y1 > LCD_HEIGHT_PX - 1) y1 = LCD_HEIGHT_PX - 1;
	hwFillRect(0, y0, LCD_WIDTH_PX, y1 - y0 + 1, (unsigned short)CON_PAPER);
}

static void hwDrawRow(int r);

/* Draw one character into its own cell.
 *
 * The first version of this file repainted the whole row for every character
 * it was handed.  That is fine for an interactive prompt, but a game that
 * redraws its map on every frame turns it into tens of thousands of font
 * syscalls per frame and the calculator slows to a crawl.  Painting a single
 * cell keeps a full redraw down to one call per character, which is the
 * difference between a slideshow and something that runs. */
static void hwDrawCell(int r, int c) {
	if (r < 0 || r >= CON_ROWS || c < 0 || c >= CON_COLS) return;

#if PRIZM_CON_FONT == 1
	/* The 24 px font is addressed by text cell, not by pixel, so a single
	   character cannot be painted on its own: one call draws the whole line.
	   That is still only one syscall per row, so it is cheap enough. */
	hwDrawRow(r);
#else
	if (!g_vram) g_vram = (unsigned short*)GetVRAMAddress();
	int x0 = c * COL_W;
	int y0 = ROW_TOP + r * ROW_H;
	/* Wipe just this cell: a blank glyph does not necessarily cover the box
	   the previous character left behind. */
	hwFillRect(x0, y0, COL_W, ROW_H, (unsigned short)CON_PAPER);

	char one[2];
	one[0] = g_scr[r][c] ? g_scr[r][c] : ' ';
	one[1] = 0;
	int x = x0;
	int y = y0;
	/* mode 0x40: draw at exactly y, without the status area offset. */
#  if PRIZM_CON_FONT == 3
	/* PrintMiniMini takes no back_color argument, and the OS paints its
	   default background, which is white, behind every glyph.  On a black
	   paper that turns each cell into a white block and the text becomes
	   unreadable, so the back colour has to be asked for through the mode
	   bits: 0x04 is "write with a black back colour", which also inverts the
	   font to white, and 0x80 lets the background we already filled
	   ourselves survive.  A black paper therefore costs the small font its
	   coloured text, it can only be black and white. */
#    if PRIZM_CON_BG
	PrintMiniMini(&x, &y, one, 0x40 | 0x04 | 0x80, TEXT_COLOR_BLACK, 0);
#    else
	PrintMiniMini(&x, &y, one, 0x40, (char)conIdx(g_fg), 0);
#    endif
#  else
	/* writeflag 1: really draw instead of only measuring the width. */
	PrintMini(&x, &y, one, 0x40, LCD_WIDTH_PX, 0, 0,
	          (int)CON_PAL[g_fg & 7], (int)CON_PAPER, 1, 0);
#  endif
#endif
}

static void hwDrawRow(int r) {
	if (r < 0 || r >= CON_ROWS) return;
	int y0 = ROW_TOP + r * ROW_H;
	/* Wipe the row first. Drawing a space per cell would cover the glyph
	   boxes, but not the gaps between glyphs, so old pixels would survive
	   a shorter redraw. */
	hwClearBand(y0, y0 + ROW_H - 1);

#if PRIZM_CON_FONT == 1

	/* The 24 px font wants cell coordinates instead of pixels, and it eats the
	   first two bytes of the string, so two filler bytes go in front. */
	char buf[CON_COLS + 3];
	buf[0] = ' ';
	buf[1] = ' ';
	for (int i = 0; i < CON_COLS; i++) {
		char c = g_scr[r][i];
		buf[2 + i] = c ? c : ' ';
	}
	buf[2 + CON_COLS] = 0;
	PrintXY(1, r + 1, buf, TEXT_MODE_TRANSPARENT_BACKGROUND, conIdx(g_fg));

#else

	for (int c = 0; c < CON_COLS; c++) hwDrawCell(r, c);

#endif
}

static void hwPushRow(int r) {
	int y0 = ROW_TOP + r * ROW_H;
	int y1 = y0 + ROW_H - 1;
	if (y1 > LCD_HEIGHT_PX - 1) y1 = LCD_HEIGHT_PX - 1;
	Bdisp_PutDisp_DD_stripe(y0, y1);
}

static void hwDrawAll(void) {
	for (int r = 0; r < CON_ROWS; r++) hwDrawRow(r);
	Bdisp_PutDisp_DD();
}

static void conPutc(char c);

static void conNewline(void) {
	g_col = 0;
	g_row++;
	if (g_row >= CON_VROWS) {
		/* Out of virtual rows: scroll the whole buffer up one line. The
		   visible window sits at the top, so this is the only path that
		   really has to repaint everything. */
		for (int r = 1; r < CON_VROWS; r++)
			for (int i = 0; i < CON_COLS; i++)
				g_scr[r - 1][i] = g_scr[r][i];
		for (int i = 0; i < CON_COLS; i++) g_scr[CON_VROWS - 1][i] = 0;
		g_row = CON_VROWS - 1;
		hwDrawAll();
	} else {
		for (int i = 0; i < CON_COLS; i++) g_scr[g_row][i] = 0;
		/* Rows below the visible window are kept in memory but not painted. */
		if (g_row < CON_ROWS) {
			hwDrawRow(g_row);
			hwPushRow(g_row);
		}
	}
}

static void conPutc(char c) {
	if (!g_inited) { g_inited = 1; hwInit(); }
	g_used = 1;

	if (c == '\r') { g_col = 0; return; }
	if (c == '\n') { conNewline(); return; }
	if (c == '\b') {
		if (g_col > 0) {
			g_col--;
			g_scr[g_row][g_col] = 0;
			hwDrawCell(g_row, g_col);
			if (g_row < CON_ROWS) hwPushRow(g_row);
		}
		return;
	}
	if (c == '\t') {
		int n = 4 - (g_col & 3);
		while (n-- > 0) conPutc(' ');
		return;
	}
	if ((unsigned char)c < 0x20) return;

	g_scr[g_row][g_col] = c;
	int r = g_row;
	int cc = g_col;
	hwDrawCell(r, cc);
	if (r < CON_ROWS) hwPushRow(r);
	g_col++;
	if (g_col >= CON_COLS) {
		conNewline();
		return;
	}
}

static void conWrite(const char* s) {
	if (!s) s = "(null)";
	while (*s) conPutc(*s++);
}

static int conStrLen(const char* s) { int n = 0; if (s) while (s[n]) n++; return n; }

extern "C" void prizm_con_end(void) {
	if (!g_used) return;
	if (g_col != 0) conPutc('\n');
	conWrite("-- Press any key --");
	Bdisp_PutDisp_DD();
	int key = 0;
	for (;;) {
		GetKey(&key);
		if (key != 0) break;
	}
	Bdisp_PutDisp_DD();
}

/* ---- hooks for the library shims (windows.h, system(), stdio) ------------
   A program written for a Windows console cannot reach into this file, so
   these are the few entry points prizm_rt.cpp forwards to. */

extern "C" void prizm_con_write(const char* s) { conWrite(s); }
extern "C" int  prizm_con_used(void) { return g_used; }

/* Width of the text window. Not virtual: the buffer is exactly this wide, so
   a program that wraps at this column gets the wrap it asked for. */
extern "C" int  prizm_con_cols(void) { return CON_COLS; }

/* Height reported to the program, see CON_VROWS. */
extern "C" int  prizm_con_rows(void) { return CON_VROWS; }

extern "C" void prizm_con_goto(int col, int row) {
	if (col < 0) col = 0;
	if (row < 0) row = 0;
	if (col > CON_COLS - 1) col = CON_COLS - 1;
	if (row > CON_VROWS - 1) row = CON_VROWS - 1;
	g_col = col;
	g_row = row;
}

extern "C" void prizm_con_cursor(int visible) { g_cursorVisible = visible ? 1 : 0; }
extern "C" int  prizm_con_cursor_state(void) { return g_cursorVisible; }

extern "C" void prizm_con_flush(void) { Bdisp_PutDisp_DD(); }

/* system("cls"): blank the whole text window and park the cursor at home. */
extern "C" void prizm_con_clear(void) {
	for (int r = 0; r < CON_VROWS; r++)
		for (int i = 0; i < CON_COLS; i++)
			g_scr[r][i] = 0;
	g_col = 0;
	g_row = 0;
	if (g_inited) {
		hwClearBand(0, LCD_HEIGHT_PX - 1);
		Bdisp_PutDisp_DD();
	}
}

static int fmtULL(unsigned long long v, char* out) {
	if (v == 0) { out[0] = '0'; out[1] = 0; return 1; }
	char tmp[24];
	int n = 0;
	while (v) { tmp[n++] = (char)('0' + (int)(v % 10ull)); v /= 10ull; }
	for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
	out[n] = 0;
	return n;
}

static int fmtLL(long long v, char* out) {
	if (v < 0) {
		out[0] = '-';
		unsigned long long uv = (unsigned long long)(-(v + 1)) + 1ull;
		return 1 + fmtULL(uv, out + 1);
	}
	return fmtULL((unsigned long long)v, out);
}

static int fmtHex(unsigned long long v, char* out, int upper) {
	char tmp[20];
	int n = 0;
	if (v == 0) tmp[n++] = '0';
	while (v) {
		int d = (int)(v & 0xF);
		tmp[n++] = (char)(d < 10 ? ('0' + d) : ((upper ? 'A' : 'a') + d - 10));
		v >>= 4;
	}
	for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
	out[n] = 0;
	return n;
}

static unsigned long long ptrVal(const void* p) {
	return (unsigned long long)(size_t)p;
}

static int fmtOct(unsigned long long v, char* out) {
	char tmp[24];
	int n = 0;
	if (v == 0) tmp[n++] = '0';
	while (v) { tmp[n++] = (char)('0' + (int)(v & 7)); v >>= 3; }
	for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
	out[n] = 0;
	return n;
}

static int fmtDouble(double v, char* out, int prec, int stripZeros) {
	int n = 0;
	if (v != v) { out[0] = 'n'; out[1] = 'a'; out[2] = 'n'; out[3] = 0; return 3; }
	if (v < 0) { out[n++] = '-'; v = -v; }
	if (v > 1.0e12) { out[n++] = 'i'; out[n++] = 'n'; out[n++] = 'f'; out[n] = 0; return n; }

	if (prec < 0) prec = 0;
	if (prec > 9) prec = 9;
	unsigned long long scale = 1ull;
	for (int i = 0; i < prec; i++) scale *= 10ull;

	unsigned long long total = (unsigned long long)(v * (double)scale + 0.5);
	unsigned long long ip = total / scale;
	unsigned long long fp = total % scale;

	n += fmtULL(ip, out + n);

	if (prec > 0) {
		char tmp[12];

		for (int i = 0; i < prec; i++) { tmp[i] = (char)('0' + (int)(fp % 10ull)); fp /= 10ull; }
		int start = 0;
		if (stripZeros) { while (start < prec && tmp[start] == '0') start++; }
		int len = prec - start;
		if (len > 0) {
			out[n++] = '.';
			for (int i = prec - 1; i >= start; i--) out[n++] = tmp[i];
		}
	}
	out[n] = 0;
	return n;
}

namespace std {

ostream cout;
ostream cerr;
ostream clog;

ostream& ostream::put(char c) { conPutc(c); return *this; }

ostream& ostream::write(const char* s, long n) {
	for (long i = 0; i < n && s[i]; i++) conPutc(s[i]);
	return *this;
}

ostream& ostream::flush() { Bdisp_PutDisp_DD(); return *this; }

ostream& ostream::textcolor(int c) { g_fg = c; return *this; }
ostream& ostream::bgcolor(int)    { return *this; }

ostream& ostream::operator<<(bool v) {
	if (m_flags & __ios_boolalpha) {
		if (v) return __emit("true", 4);
		return __emit("false", 5);
	}
	return *this << (int)(v ? 1 : 0);
}

/* Integer output honors basefield, uppercase, showbase and showpos, then
   lets __emit handle width/fill/alignment. */
ostream& ostream::operator<<(int v) {
	char b[32];
	int n = 0;
	if (m_flags & __ios_hex) {
		if (m_flags & __ios_showbase) { b[n++] = '0'; b[n++] = (m_flags & __ios_uppercase) ? 'X' : 'x'; }
		n += fmtHex((unsigned int)v, b + n, (m_flags & __ios_uppercase) ? 1 : 0);
	} else if (m_flags & __ios_oct) {
		if (m_flags & __ios_showbase) b[n++] = '0';
		n += fmtOct((unsigned int)v, b + n);
	} else {
		if (v < 0) { b[n++] = '-'; n += fmtULL((unsigned long long)(-(long long)v), b + n); }
		else {
			if (m_flags & __ios_showpos) b[n++] = '+';
			n += fmtULL((unsigned long long)v, b + n);
		}
	}
	b[n] = 0;
	return __emit(b, n);
}

ostream& ostream::operator<<(unsigned int v) {
	char b[32];
	int n = 0;
	if (m_flags & __ios_hex) {
		if (m_flags & __ios_showbase) { b[n++] = '0'; b[n++] = (m_flags & __ios_uppercase) ? 'X' : 'x'; }
		n += fmtHex(v, b + n, (m_flags & __ios_uppercase) ? 1 : 0);
	} else if (m_flags & __ios_oct) {
		if (m_flags & __ios_showbase) b[n++] = '0';
		n += fmtOct(v, b + n);
	} else {
		if (m_flags & __ios_showpos) b[n++] = '+';
		n += fmtULL(v, b + n);
	}
	b[n] = 0;
	return __emit(b, n);
}

ostream& ostream::operator<<(long long v) {
	char b[32];
	int n = 0;
	if (m_flags & __ios_hex) {
		if (m_flags & __ios_showbase) { b[n++] = '0'; b[n++] = (m_flags & __ios_uppercase) ? 'X' : 'x'; }
		n += fmtHex((unsigned long long)v, b + n, (m_flags & __ios_uppercase) ? 1 : 0);
	} else if (m_flags & __ios_oct) {
		if (m_flags & __ios_showbase) b[n++] = '0';
		n += fmtOct((unsigned long long)v, b + n);
	} else {
		if (v < 0) { b[n++] = '-'; n += fmtULL((unsigned long long)(-v), b + n); }
		else {
			if (m_flags & __ios_showpos) b[n++] = '+';
			n += fmtULL((unsigned long long)v, b + n);
		}
	}
	b[n] = 0;
	return __emit(b, n);
}

ostream& ostream::operator<<(unsigned long long v) {
	char b[32];
	int n = 0;
	if (m_flags & __ios_hex) {
		if (m_flags & __ios_showbase) { b[n++] = '0'; b[n++] = (m_flags & __ios_uppercase) ? 'X' : 'x'; }
		n += fmtHex(v, b + n, (m_flags & __ios_uppercase) ? 1 : 0);
	} else if (m_flags & __ios_oct) {
		if (m_flags & __ios_showbase) b[n++] = '0';
		n += fmtOct(v, b + n);
	} else {
		if (m_flags & __ios_showpos) b[n++] = '+';
		n += fmtULL(v, b + n);
	}
	b[n] = 0;
	return __emit(b, n);
}

/* Scientific notation: d.ddd e+xx.  The mantissa is normalized to [1,10)
   digit by digit so it works without libm log10. */
static int fmtSci(double v, char* out, int prec, int upper) {
	int n = 0;
	if (v != v) { out[0] = 'n'; out[1] = 'a'; out[2] = 'n'; out[3] = 0; return 3; }
	if (v < 0) { out[n++] = '-'; v = -v; }
	int e = 0;
	if (v > 0) {
		while (v >= 10.0) { v /= 10.0; e++; }
		while (v < 1.0)   { v *= 10.0; e--; }
	}
	n += fmtDouble(v, out + n, prec, 0);
	out[n++] = upper ? 'E' : 'e';
	if (e < 0) { out[n++] = '-'; e = -e; } else out[n++] = '+';
	char eb[4];
	int m = 0;
	do { eb[m++] = (char)('0' + e % 10); e /= 10; } while (e);
	while (m < 2) eb[m++] = '0';          /* at least two exponent digits */
	for (int i = m - 1; i >= 0; i--) out[n++] = eb[i];
	out[n] = 0;
	return n;
}

ostream& ostream::operator<<(double v) {
	char b[48];
	int n;
	if (m_flags & __ios_sci) {
		n = fmtSci(v, b, m_prec, (m_flags & __ios_uppercase) ? 1 : 0);
	} else if (m_flags & __ios_fixed) {
		fmtDouble(v, b, m_prec, 0);
		n = (int)__builtin_strlen(b);
	} else {
		fmtDouble(v, b, m_prec, 1);
		n = (int)__builtin_strlen(b);
	}
	return __emit(b, n);
}

ostream& ostream::operator<<(const void* p) {
	char b[24];
	int n = 0;
	b[n++] = '0'; b[n++] = (m_flags & __ios_uppercase) ? 'X' : 'x';
	n += fmtHex(ptrVal(p), b + n, (m_flags & __ios_uppercase) ? 1 : 0);
	b[n] = 0;
	return __emit(b, n);
}

ostream& endl(ostream& os)  { os.put('\n'); os.flush(); return os; }
ostream& flush(ostream& os) { os.flush(); return os; }
ostream& ends(ostream& os)  { return os; }

}

static int conReadLine(char* buf, int max) {
	int n = 0;
	int key = 0;
	for (;;) {
		GetKey(&key);
		if (key == KEY_CTRL_EXE) break;
		if (key == KEY_CTRL_EXIT || key == KEY_CTRL_AC) {
			conPutc('\n');
			buf[0] = 0;
			return -1;
		}
		if (key == KEY_CTRL_DEL) {
			if (n > 0) { n--; conPutc('\b'); }
			continue;
		}
		char ch = 0;
		if (key >= 0x20 && key <= 0x7E)      ch = (char)key;
		else if (key == 0x87 || key == 0x99) ch = '-';
		if (ch && n < max - 1) { buf[n++] = ch; conPutc(ch); }
	}
	conPutc('\n');
	buf[n] = 0;
	return n;
}

static char g_tok[160];
static int  g_tokPos = 0;
static int  g_tokLen = 0;
static char g_word[160];

static int nextToken(char* out, int max) {
	for (;;) {
		while (g_tokPos < g_tokLen && (g_tok[g_tokPos] == ' ' || g_tok[g_tokPos] == '\t')) g_tokPos++;
		if (g_tokPos < g_tokLen) break;
		int n = conReadLine(g_tok, (int)sizeof(g_tok) - 1);
		if (n < 0) { out[0] = 0; return -1; }
		g_tokLen = n;
		g_tokPos = 0;
	}
	int n = 0;
	while (g_tokPos < g_tokLen && g_tok[g_tokPos] != ' ' && g_tok[g_tokPos] != '\t' && n < max - 1)
		out[n++] = g_tok[g_tokPos++];
	out[n] = 0;
	return n;
}

static int parseLL(const char* s, long long* out) {
	int i = 0, neg = 0;
	while (s[i] == ' ' || s[i] == '\t') i++;
	if (s[i] == '+' || s[i] == '-') { neg = (s[i] == '-'); i++; }
	if (s[i] < '0' || s[i] > '9') return 0;
	unsigned long long v = 0;
	while (s[i] >= '0' && s[i] <= '9') { v = v * 10ull + (unsigned)(s[i] - '0'); i++; }
	*out = neg ? -(long long)v : (long long)v;
	return 1;
}

static int parseULL(const char* s, unsigned long long* out) {
	int i = 0;
	while (s[i] == ' ' || s[i] == '\t') i++;
	if (s[i] == '+') i++;
	if (s[i] < '0' || s[i] > '9') return 0;
	unsigned long long v = 0;
	while (s[i] >= '0' && s[i] <= '9') { v = v * 10ull + (unsigned)(s[i] - '0'); i++; }
	*out = v;
	return 1;
}

static int parseDouble(const char* s, double* out) {
	int i = 0, neg = 0, any = 0;
	double v = 0.0;
	while (s[i] == ' ' || s[i] == '\t') i++;
	if (s[i] == '+' || s[i] == '-') { neg = (s[i] == '-'); i++; }
	while (s[i] >= '0' && s[i] <= '9') { v = v * 10.0 + (double)(s[i] - '0'); i++; any = 1; }
	if (s[i] == '.') {
		i++;
		double f = 0.1;
		while (s[i] >= '0' && s[i] <= '9') { v += (double)(s[i] - '0') * f; f *= 0.1; i++; any = 1; }
	}
	if (!any) return 0;
	*out = neg ? -v : v;
	return 1;
}

namespace std {

istream cin;

/* ---- istream virtual hooks (console source) ----------------------- */

int istream::__next_token(char* out, int max) {
	return nextToken(out, max);
}

int istream::__read_char() {
	int key = 0;
	for (;;) {
		GetKey(&key);
		if (key >= 0x20 && key <= 0x7E) { conPutc((char)key); return key; }
		if (key == KEY_CTRL_EXE) { conPutc('\n'); return '\n'; }
		if (key == KEY_CTRL_EXIT) return -1;
	}
}

void istream::__read_line(char* s, int n, char delim) {
	int i = 0;
	int key = 0;
	for (;;) {
		GetKey(&key);
		if (key == KEY_CTRL_EXE) break;
		if (key == KEY_CTRL_EXIT) { m_fail = 1; break; }
		if (key == KEY_CTRL_DEL) { if (i > 0) { i--; conPutc('\b'); } continue; }
		char ch = 0;
		if (key >= 0x20 && key <= 0x7E)      ch = (char)key;
		else if (key == 0x87 || key == 0x99) ch = '-';
		if (!ch) continue;
		if (ch == delim) break;
		if (i < n - 1) { s[i++] = ch; conPutc(ch); }
	}
	s[i] = 0;
	conPutc('\n');
}

void istream::__skip_ws() {
	while (g_tokPos < g_tokLen && (g_tok[g_tokPos] == ' ' || g_tok[g_tokPos] == '\t')) g_tokPos++;
}

/* ---- formatted extraction ----------------------------------------- */

istream& istream::operator>>(char* s) {
	int n = __next_token(s, 64);
	if (n <= 0) m_fail = 1;
	return *this;
}

istream& istream::operator>>(char& c) {
	int n = __next_token(g_word, sizeof(g_word));
	if (n <= 0) { m_fail = 1; return *this; }
	c = g_word[0];
	return *this;
}

istream& istream::operator>>(int& v) {
	long long t = 0;
	if (__next_token(g_word, sizeof(g_word)) < 0 || !parseLL(g_word, &t)) { m_fail = 1; return *this; }
	v = (int)t;
	return *this;
}

istream& istream::operator>>(unsigned int& v) {
	unsigned long long t = 0;
	if (__next_token(g_word, sizeof(g_word)) < 0 || !parseULL(g_word, &t)) { m_fail = 1; return *this; }
	v = (unsigned int)t;
	return *this;
}

istream& istream::operator>>(long long& v) {
	if (__next_token(g_word, sizeof(g_word)) < 0 || !parseLL(g_word, &v)) m_fail = 1;
	return *this;
}

istream& istream::operator>>(unsigned long long& v) {
	if (__next_token(g_word, sizeof(g_word)) < 0 || !parseULL(g_word, &v)) m_fail = 1;
	return *this;
}

istream& istream::operator>>(double& v) {
	if (__next_token(g_word, sizeof(g_word)) < 0 || !parseDouble(g_word, &v)) m_fail = 1;
	return *this;
}

istream& endl(istream& is) { return is; }

}

typedef void (*CharSink)(char c, void* ctx);

static void sinkConsole(char c, void* ctx) { (void)ctx; conPutc(c); }

struct BufSink { char* p; int n; };
static void sinkBuf(char c, void* ctx) {
	BufSink* b = (BufSink*)ctx;
	b->p[b->n++] = c;
}

static int emitStr(CharSink sink, void* ctx, const char* s, int len,
                   int width, int leftAlign, char pad) {
	if (len < 0) len = 0;
	int written = 0;
	int pads = (width > len) ? width - len : 0;
	if (!leftAlign) for (int i = 0; i < pads; i++) { sink(pad, ctx); written++; }
	for (int i = 0; i < len; i++) { sink(s[i], ctx); written++; }
	if (leftAlign) for (int i = 0; i < pads; i++) { sink(pad, ctx); written++; }
	return written;
}

static int emitNum(CharSink sink, void* ctx, char signCh, const char* digits, int len,
                   int width, int leftAlign, int zeroPad, int precision) {
	char tmp[64];
	int tn = 0;
	if (precision > 0) {
		while (tn < precision - len && tn < 60) tmp[tn++] = '0';
	}
	if (!(precision == 0 && len == 1 && digits[0] == '0')) {
		for (int i = 0; i < len && tn < 62; i++) tmp[tn++] = digits[i];
	}

	int bodyLen = tn + (signCh ? 1 : 0);
	int written = 0;
	int pads = (width > bodyLen) ? width - bodyLen : 0;

	if (leftAlign) {
		if (signCh) { sink(signCh, ctx); written++; }
		for (int i = 0; i < tn; i++) { sink(tmp[i], ctx); written++; }
		for (int i = 0; i < pads; i++) { sink(' ', ctx); written++; }
	} else if (zeroPad) {
		if (signCh) { sink(signCh, ctx); written++; }
		for (int i = 0; i < pads; i++) { sink('0', ctx); written++; }
		for (int i = 0; i < tn; i++) { sink(tmp[i], ctx); written++; }
	} else {
		for (int i = 0; i < pads; i++) { sink(' ', ctx); written++; }
		if (signCh) { sink(signCh, ctx); written++; }
		for (int i = 0; i < tn; i++) { sink(tmp[i], ctx); written++; }
	}
	return written;
}

static int coreFormat(CharSink sink, void* ctx, const char* fmt, va_list ap) {
	int n = 0;
	if (!fmt) return 0;

	for (const char* p = fmt; *p; p++) {
		if (*p != '%') { sink(*p, ctx); n++; continue; }
		p++;
		if (!*p) break;
		if (*p == '%') { sink('%', ctx); n++; continue; }

		int leftAlign = 0, zeroPad = 0, plusSign = 0, spaceSign = 0;
		for (;; p++) {
			if (*p == '-')      leftAlign = 1;
			else if (*p == '0') zeroPad = 1;
			else if (*p == '+') plusSign = 1;
			else if (*p == ' ') spaceSign = 1;
			else if (*p == '#') {  }
			else break;
		}

		int width = 0;
		if (*p == '*') {
			width = va_arg(ap, int); p++;
			if (width < 0) { leftAlign = 1; width = -width; }
		} else {
			while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
		}

		int prec = -1;
		if (*p == '.') {
			p++; prec = 0;
			if (*p == '*') { prec = va_arg(ap, int); p++; if (prec < 0) prec = -1; }
			else while (*p >= '0' && *p <= '9') { prec = prec * 10 + (*p - '0'); p++; }
		}

		int cntL = 0;
		while (*p == 'l' || *p == 'h' || *p == 'z' || *p == 'j' || *p == 't' || *p == 'L') {
			if (*p == 'l') cntL++;
			p++;
		}
		if (!*p) break;

		char num[72];
		switch (*p) {
		case 'c':
			{ char cc = (char)va_arg(ap, int);
			  n += emitStr(sink, ctx, &cc, 1, width, leftAlign, ' '); }
			break;

		case 's':
			{ const char* s = va_arg(ap, const char*);
			  if (!s) s = "(null)";
			  int len = conStrLen(s);
			  if (prec >= 0 && len > prec) len = prec;
			  n += emitStr(sink, ctx, s, len, width, leftAlign, ' '); }
			break;

		case 'd': case 'i': {
			long long v;
			if (cntL >= 2)      v = va_arg(ap, long long);
			else if (cntL == 1) v = (long long)va_arg(ap, long);
			else                v = (long long)va_arg(ap, int);
			char sign = 0;
			unsigned long long uv;
			if (v < 0) { sign = '-'; uv = (unsigned long long)(-(v + 1)) + 1ull; }
			else { uv = (unsigned long long)v; if (plusSign) sign = '+'; else if (spaceSign) sign = ' '; }
			int len = fmtULL(uv, num);
			n += emitNum(sink, ctx, sign, num, len, width, leftAlign, zeroPad, prec);
			break;
		}

		case 'u': case 'o': case 'x': case 'X': {
			unsigned long long v;
			if (cntL >= 2)      v = va_arg(ap, unsigned long long);
			else if (cntL == 1) v = (unsigned long long)va_arg(ap, unsigned long);
			else                v = (unsigned long long)va_arg(ap, unsigned int);
			int len;
			if (*p == 'u')      len = fmtULL(v, num);
			else if (*p == 'o') len = fmtOct(v, num);
			else                len = fmtHex(v, num, (*p == 'X'));
			n += emitNum(sink, ctx, 0, num, len, width, leftAlign, zeroPad, prec);
			break;
		}

		case 'p': {
			unsigned long long v = ptrVal(va_arg(ap, void*));
			num[0] = '0'; num[1] = 'x';
			int len = 2 + fmtHex(v, num + 2, 0);
			n += emitStr(sink, ctx, num, len, width, leftAlign, ' ');
			break;
		}

		case 'f': case 'g': {
			double v = va_arg(ap, double);
			int pe = (prec >= 0) ? prec : 6;
			int len = fmtDouble(v, num, pe, (*p == 'g') ? 1 : 0);
			n += emitStr(sink, ctx, num, len, width, leftAlign, zeroPad ? '0' : ' ');
			break;
		}

		default:
			sink('%', ctx); sink(*p, ctx); n += 2;
			break;
		}
	}
	return n;
}

extern "C" {

/* The whole printf family is taken over below, because a calculator has no
   stdout: the SDK's own printf has nowhere to send its characters, so the
   console claims the names and paints the text onto the screen instead.

   On a PC build that is exactly the wrong thing to do - the test harness in
   host-src\consoletest compiles this very file and prints its report with
   fputs / vsnprintf, so a weak override here would swallow the report into the
   simulated screen and the run would look silently empty.  The overrides are
   therefore limited to the calculator, which is what the compiler builtin
   __sh__ identifies.  Nothing in the builder has to pass a flag for this.

   The console keeps its own formatter either way, so sprintf and friends still
   behave identically on both sides. */
#if defined(__sh__)

__attribute__((weak))
int printf(const char* fmt, ...) {
	va_list ap; va_start(ap, fmt);
	int n = coreFormat(sinkConsole, 0, fmt, ap);
	va_end(ap);
	return n;
}

__attribute__((weak))
int vprintf(const char* fmt, va_list ap) {
	return coreFormat(sinkConsole, 0, fmt, ap);
}

__attribute__((weak))
int fprintf(FILE* stream, const char* fmt, ...) {
	(void)stream;
	va_list ap; va_start(ap, fmt);
	int n = coreFormat(sinkConsole, 0, fmt, ap);
	va_end(ap);
	return n;
}

__attribute__((weak))
int vfprintf(FILE* stream, const char* fmt, va_list ap) {
	(void)stream;
	return coreFormat(sinkConsole, 0, fmt, ap);
}

__attribute__((weak))
int sprintf(char* dest, const char* fmt, ...) {
	BufSink b; b.p = dest; b.n = 0;
	va_list ap; va_start(ap, fmt);
	coreFormat(sinkBuf, &b, fmt, ap);
	va_end(ap);
	dest[b.n] = 0;
	return b.n;
}

__attribute__((weak))
int vsprintf(char* dest, const char* fmt, va_list ap) {
	BufSink b; b.p = dest; b.n = 0;
	coreFormat(sinkBuf, &b, fmt, ap);
	dest[b.n] = 0;
	return b.n;
}

__attribute__((weak))
int puts(const char* s) {
	conWrite(s);
	conPutc('\n');
	return 0;
}

__attribute__((weak))
int putchar(int c) {
	conPutc((char)c);
	return c;
}

/* Bounded formatting. The capped sink keeps counting so the return value still
   matches the C standard, but it stores at most n - 1 characters plus a
   terminator, which is what keeps a long string from running off the end of
   the caller's buffer. */
struct CapSink { char* p; int n; int cap; };

static void sinkCap(char c, void* ctx) {
	CapSink* b = (CapSink*)ctx;
	if (b->p && b->n < b->cap - 1) b->p[b->n] = c;
	b->n++;
}

__attribute__((weak))
int vsnprintf(char* dest, size_t n, const char* fmt, va_list ap) {
	if (!dest || n == 0) return 0;
	CapSink b; b.p = dest; b.n = 0; b.cap = (int)n;
	int total = coreFormat(sinkCap, &b, fmt, ap);
	int w = b.n;
	if (w > (int)n - 1) w = (int)n - 1;
	if (w < 0) w = 0;
	dest[w] = 0;
	return total;
}

__attribute__((weak))
int snprintf(char* dest, size_t n, const char* fmt, ...) {
	va_list ap; va_start(ap, fmt);
	int r = vsnprintf(dest, n, fmt, ap);
	va_end(ap);
	return r;
}

__attribute__((weak))
int fputs(const char* s, FILE* stream) {
	(void)stream;
	conWrite(s);
	return 0;
}

#endif /* __sh__ : end of the calculator-only stdio takeover */

/* cassert sends failures here. There is nowhere to print a message in the
   normal sense, so it goes on the console and waits, the same way the end of a
   program does. */
void prizm_assert_fail(const char* expr, const char* file, int line) {
	(void)file;
	(void)line;
	conWrite("\nassert failed: ");
	conWrite(expr ? expr : "?");
	conWrite("\n");
	Bdisp_PutDisp_DD();
	int key = 0;
	for (;;) { GetKey(&key); if (key != 0) break; }
}

}

extern "C" {

#define PB_PREFIX (-2)
#define PB_MAX    8

static int g_pb[PB_MAX];
static int g_pbHead = 0;
static int g_pbN = 0;

static void pbPush(int v) {
	if (g_pbN >= PB_MAX) return;
	g_pb[(g_pbHead + g_pbN) % PB_MAX] = v;
	g_pbN++;
}

static int pbPop(void) {
	if (g_pbN <= 0) return -1;
	int v = g_pb[g_pbHead];
	g_pbHead = (g_pbHead + 1) % PB_MAX;
	g_pbN--;
	return v;
}

static void pbPushFront(int v) {
	if (g_pbN >= PB_MAX) return;
	g_pbHead = (g_pbHead - 1 + PB_MAX) % PB_MAX;
	g_pb[g_pbHead] = v;
	g_pbN++;
}

static int pzkIsModifier(int kc) {
	return kc == KEY_CTRL_SHIFT || kc == KEY_CTRL_ALPHA;
}

static int pzkExtend(int kc) {
	switch (kc) {
	case KEY_CTRL_UP:    return 72;
	case KEY_CTRL_DOWN:  return 80;
	case KEY_CTRL_LEFT:  return 75;
	case KEY_CTRL_RIGHT: return 77;
	case KEY_CTRL_F1:    return 59;
	case KEY_CTRL_F2:    return 60;
	case KEY_CTRL_F3:    return 61;
	case KEY_CTRL_F4:    return 62;
	case KEY_CTRL_F5:    return 63;
	case KEY_CTRL_F6:    return 64;
	default: return 0;
	}
}

static int pzkToConio(int kc, int* out) {
	if (kc >= 0x20 && kc <= 0x7E) { out[0] = kc; return 1; }

	switch (kc) {
	case KEY_CHAR_PLUS:   out[0] = '+'; return 1;
	case KEY_CHAR_MINUS:  out[0] = '-'; return 1;
	case KEY_CHAR_PMINUS: out[0] = '-'; return 1;
	case KEY_CHAR_MULT:   out[0] = '*'; return 1;
	case KEY_CHAR_DIV:    out[0] = '/'; return 1;
	case KEY_CHAR_FRAC:   out[0] = '/'; return 1;
	case KEY_CHAR_POW:    out[0] = '^'; return 1;
	case KEY_CHAR_EXP:    out[0] = 'E'; return 1;
	case KEY_CHAR_CR:     out[0] = 13;  return 1;
	default: break;
	}

	switch (kc) {
	case KEY_CTRL_EXE:  out[0] = 13; return 1;
	case KEY_CTRL_DEL:  out[0] = 8;  return 1;
	case KEY_CTRL_EXIT: out[0] = 27; return 1;
	case KEY_CTRL_AC:   out[0] = 3;  return 1;
	default: break;
	}

	int ext = pzkExtend(kc);
	if (ext) { out[0] = PB_PREFIX; out[1] = ext; return 2; }
	return 0;
}

static int pzkHasKey(void) {
	int col = 0, row = 0;
	unsigned short kc = 0;
	int ev = GetKeyWait_OS(&col, &row, KEYWAIT_HALTOFF_TIMEROFF, 0, 1, &kc);
	return ev == KEYREP_KEYEVENT;
}

static int pzkTakeKey(void) {
	int ent[2];
	for (;;) {
		int kc = 0;
		GetKey(&kc);
		if (pzkIsModifier(kc)) continue;
		int n = pzkToConio(kc, ent);
		if (n <= 0) continue;
		for (int i = 1; i < n; i++) pbPush(ent[i]);
		return ent[0];
	}
}

static int pzkNextRaw(void) {
	if (g_pbN > 0) return pbPop();
	return pzkTakeKey();
}

int _kbhit(void) {
	if (g_pbN > 0) return 1;
	return pzkHasKey();
}

int _getch(void) {
	int v = pzkNextRaw();
	if (v == PB_PREFIX) return 0;
	return v;
}

int _getwch(void) {
	int v = pzkNextRaw();
	if (v == PB_PREFIX) return 0xE0;
	return v;
}

int _getche(void) {
	int c = _getch();
	if (c >= 0x20 && c < 0x7F) conPutc((char)c);
	else if (c == 13) conPutc('\n');
	else if (c == 8) conPutc('\b');
	return c;
}

int _getwche(void) {
	int c = _getwch();
	if (c >= 0x20 && c < 0x7F) conPutc((char)c);
	else if (c == 13) conPutc('\n');
	else if (c == 8) conPutc('\b');
	return c;
}

int _ungetch(int c) {
	pbPushFront(c & 0xFF);
	return c;
}

int _ungetwch(int c) { return _ungetch(c); }

int _putch(int c) { conPutc((char)c); return c; }

int _putwch(int c) { conPutc((char)c); return c; }

int _cputs(const char* s) { conWrite(s); return 0; }

int _putws(const wchar_t* s) {
	if (!s) return 0;
	while (*s) conPutc((char)(*s++ & 0xFF));
	return 0;
}

int _cprintf(const char* fmt, ...) {
	va_list ap; va_start(ap, fmt);
	int n = coreFormat(sinkConsole, 0, fmt, ap);
	va_end(ap);
	return n;
}

int _cwprintf(const wchar_t* fmt, ...) {
	if (!fmt) return 0;
	char narrow[256];
	int i = 0;
	while (*fmt && i < 255) narrow[i++] = (char)(*fmt++ & 0xFF);
	narrow[i] = 0;
	va_list ap; va_start(ap, fmt);
	int n = coreFormat(sinkConsole, 0, narrow, ap);
	va_end(ap);
	return n;
}

int prizm_getkey(void) {
	int kc = 0;
	GetKey(&kc);
	return kc;
}

int prizm_keypoll(int* matrix) {
	int col = 0, row = 0;
	unsigned short kc = 0;
	int ev = GetKeyWait_OS(&col, &row, KEYWAIT_HALTOFF_TIMEROFF, 0, 1, &kc);
	if (ev != KEYREP_KEYEVENT) return 0;
	if (matrix) *matrix = ((col & 0xFF) << 8) | (row & 0xFF);
	return 1;
}

int prizm_getraw(int* matrix) {
	int col = 0, row = 0;
	unsigned short kc = 0;
	GetKeyWait_OS(&col, &row, KEYWAIT_HALTON_TIMEROFF, 0, 1, &kc);
	if (matrix) *matrix = ((col & 0xFF) << 8) | (row & 0xFF);
	return 1;
}

int prizm_key_extend(int keycode) { return pzkExtend(keycode); }

void prizm_kbd_flush(void) {
	g_pbHead = 0;
	g_pbN = 0;
}

}

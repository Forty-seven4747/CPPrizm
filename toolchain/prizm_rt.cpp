/* Backing implementations for the C++ standard library shims and for the
 * Win32 console subset in cppshim\windows.h.
 *
 * The Prizm SDK links against libc (newlib), libfxcg and libgcc only: there is
 * no libstdc++ and no libm.  A console program that came from Windows or from a
 * PC compiler therefore expects a pile of routines that simply are not there.
 * The ones that only need the heap or a syscall are already in libfxcg
 * (memset, memcpy, strlen, sys_rand, ...); this file fills in the rest.
 *
 * Every definition is weak on purpose.  On the calculator nothing else provides
 * these symbols, so the weak definitions win; if the same translation unit is
 * ever compiled for a PC (the console test harness does exactly that for other
 * files) the real C library keeps winning and nothing collides.
 */

#include <stddef.h>
#include <stdarg.h>

#include <ctime>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fxcg/rtc.h>
#include <fxcg/system.h>
#include <fxcg/keyboard.h>

#include <windows.h>

/* The text window lives in prizm_console.cpp; these are the hooks it exposes. */
extern "C" int  prizm_con_cols(void);
extern "C" int  prizm_con_rows(void);
extern "C" void prizm_con_goto(int col, int row);
extern "C" void prizm_con_clear(void);
extern "C" void prizm_con_flush(void);
extern "C" void prizm_con_cursor(int visible);
extern "C" int  prizm_con_cursor_state(void);
extern "C" void prizm_con_write(const char* s);

/* std::string::npos is a static const member.  Reading it as a constant needs
   no storage, but the moment a compiler decides it is odr-used the single
   definition has to exist somewhere, and this is the translation unit that is
   always linked into an add-in, so it goes here. */
namespace std {
const string::size_type string::npos;
}

extern "C" {

/* =====================================================================
   stdlib
   ===================================================================== */

/* libc declares rand / srand but never defines them; the system RNG behind
   sys_rand is what the calculator uses. */
__attribute__((weak))
int rand(void) { return sys_rand(); }

__attribute__((weak))
void srand(unsigned seed) { sys_srand(seed); }

/* system() understands one command. Anything else, including the "pause" that
   litters Windows console code, does nothing and reports success, so a ported
   program does not stall. */
__attribute__((weak))
int system(const char* command) {
	if (command) {
		while (*command == ' ' || *command == '\t') command++;
		char a = command[0], b = command[1], c = command[2];
		char d0 = (a >= 'a') ? (char)(a - 32) : a;
		char d1 = (b >= 'a') ? (char)(b - 32) : b;
		char d2 = (c >= 'a') ? (char)(c - 32) : c;
		if (d0 == 'C' && d1 == 'L' && d2 == 'S') prizm_con_clear();
	}
	return 0;
}

/* =====================================================================
   time

   The RTC gives a wall clock: hours, minutes, seconds and milliseconds of the
   day.  There is no calendar syscall in this SDK, so time() returns seconds
   since midnight rather than a Unix timestamp.  That is exactly enough for the
   two things console code does with it: srand(time(0)) and "how long did that
   take".  Two runs that start at the same second of the day do get the same
   seed, which is worth knowing when a game seems to repeat itself.
   ===================================================================== */

__attribute__((weak))
time_t time(time_t* t) {
	unsigned h = 0, m = 0, s = 0, ms = 0;
	RTC_GetTime(&h, &m, &s, &ms);
	time_t v = (time_t)(h * 3600u + m * 60u + s);
	if (t) *t = v;
	return v;
}

__attribute__((weak))
double difftime(time_t a, time_t b) { return (double)a - (double)b; }

static struct tm g_tm;

__attribute__((weak))
struct tm* localtime(const time_t* t) {
	unsigned v = t ? (unsigned)(*t) : 0;
	g_tm.tm_hour  = (int)((v / 3600u) % 24u);
	g_tm.tm_min   = (int)((v / 60u) % 60u);
	g_tm.tm_sec   = (int)(v % 60u);
	g_tm.tm_mday  = 0;
	g_tm.tm_mon   = 0;
	g_tm.tm_year  = 0;
	g_tm.tm_wday  = 0;
	g_tm.tm_yday  = (int)(v / 86400u);
	g_tm.tm_isdst = 0;
	return &g_tm;
}

__attribute__((weak))
struct tm* gmtime(const time_t* t) { return localtime(t); }

/* =====================================================================
   libm

   Nothing here needs to be correctly rounded, but everything needs to be
   monotonic enough for a game: clocks, distances, angles.  The reductions are
   the textbook ones, Taylor and Newton, with enough terms that the error lands
   far below what a 384 x 216 screen can show.
   ===================================================================== */

#define PRIZM_LN2   0.69314718055994530942
#define PRIZM_PI    3.14159265358979323846
#define PRIZM_PI_2  1.57079632679489661923
#define PRIZM_PI_4  0.78539816339744830962
#define PRIZM_2PI   6.28318530717958647692

__attribute__((weak))
double fabs(double x) { return x < 0.0 ? -x : x; }

__attribute__((weak))
long labs(long n) { return n < 0 ? -n : n; }

__attribute__((weak))
double floor(double x) {
	double t = (double)(long long)x;
	if (x < 0.0 && t != x) t -= 1.0;
	return t;
}

__attribute__((weak))
double ceil(double x) {
	double t = (double)(long long)x;
	if (x > 0.0 && t != x) t += 1.0;
	return t;
}

__attribute__((weak))
double trunc(double x) { return (double)(long long)x; }

__attribute__((weak))
double round(double x) {
	double f = floor(x);
	double d = x - f;
	if (d >= 0.5) f += 1.0;
	else if (d <= -0.5) f -= 1.0;
	return f;
}

__attribute__((weak))
double ldexp(double x, int e) {
	double r = x;
	while (e > 0) { r *= 2.0; e--; }
	while (e < 0) { r *= 0.5; e++; }
	return r;
}

__attribute__((weak))
double frexp(double x, int* e) {
	if (e) *e = 0;
	if (x == 0.0) return 0.0;
	int sign = 1;
	if (x < 0.0) { x = -x; sign = -1; }
	int ex = 0;
	while (x >= 1.0) { x *= 0.5; ex++; }
	while (x < 0.5)  { x *= 2.0; ex--; }
	if (e) *e = ex;
	return sign * x;
}

__attribute__((weak))
double modf(double x, double* iptr) {
	double t = trunc(x);
	if (iptr) *iptr = t;
	return x - t;
}

__attribute__((weak))
double fmod(double x, double y) {
	if (y == 0.0) return 0.0;
	double a = fabs(x), b = fabs(y);
	if (a < b) return x;
	/* Repeated subtraction after scaling keeps the precision the naive
	   "x - trunc(x / y) * y" form loses once x / y grows past 2^53. */
	double scaled = b;
	while (scaled <= a * 0.5) scaled += scaled;
	while (scaled >= b) {
		if (a >= scaled) a -= scaled;
		scaled *= 0.5;
	}
	return x < 0.0 ? -a : a;
}

__attribute__((weak))
double sqrt(double x) {
	if (x <= 0.0) return 0.0;
	double r = x > 1.0 ? x : 1.0;
	for (int i = 0; i < 60; i++) {
		double nr = 0.5 * (r + x / r);
		if (nr == r) break;
		r = nr;
	}
	return r;
}

__attribute__((weak))
double exp(double x) {
	/* x = k * ln2 + r, then a Taylor series on the small remainder. */
	double k = floor(x / PRIZM_LN2 + 0.5);
	double r = x - k * PRIZM_LN2;
	double s = 1.0, t = 1.0;
	for (int i = 1; i <= 20; i++) { t *= r / (double)i; s += t; }
	return ldexp(s, (int)k);
}

__attribute__((weak))
double log(double x) {
	if (x <= 0.0) return 0.0;
	/* x = m * 2^e with m in [1, 2), then log(m) from the atanh series. */
	int e = 0;
	while (x >= 2.0) { x *= 0.5; e++; }
	while (x < 1.0)  { x *= 2.0; e--; }
	double z = (x - 1.0) / (x + 1.0);
	double z2 = z * z;
	double s = 0.0, t = z;
	for (int i = 1; i <= 41; i += 2) { s += t / (double)i; t *= z2; }
	return 2.0 * s + (double)e * PRIZM_LN2;
}

__attribute__((weak))
double log10(double x) { return log(x) * 0.43429448190325182765; }

__attribute__((weak))
double pow(double x, double y) {
	if (y == 0.0) return 1.0;
	if (x == 0.0) return 0.0;
	double iy = floor(y);
	if (iy == y && fabs(y) < 1.0e9) {
		/* Integer exponent: exact for the repeated squaring and it keeps a
		   negative base working, which exp(y * log(x)) could not. */
		long long n = (long long)y;
		int neg = n < 0;
		if (neg) n = -n;
		double r = 1.0, b = x;
		while (n) {
			if (n & 1LL) r *= b;
			b *= b;
			n >>= 1;
		}
		return neg ? 1.0 / r : r;
	}
	if (x < 0.0) return 0.0;
	return exp(y * log(x));
}

__attribute__((weak))
double sin(double x) {
	double k = floor(x / PRIZM_2PI + 0.5);
	x -= k * PRIZM_2PI;
	double xx = x * x;
	double s = x, t = x;
	for (int i = 1; i <= 14; i++) {
		t *= -xx / (double)((2 * i) * (2 * i + 1));
		s += t;
	}
	return s;
}

__attribute__((weak))
double cos(double x) { return sin(x + PRIZM_PI_2); }

__attribute__((weak))
double tan(double x) {
	double c = cos(x);
	if (c == 0.0) return 0.0;
	return sin(x) / c;
}

/* atan by argument reduction, so the series never sees |x| > tan(pi/8). */
static double atanSeries(double x) {
	double xx = x * x;
	double s = x, t = x;
	for (int i = 1; i <= 24; i++) {
		t *= -xx;
		s += t / (double)(2 * i + 1);
	}
	return s;
}

__attribute__((weak))
double atan(double x) {
	int neg = x < 0.0;
	if (neg) x = -x;
	double r;
	if (x <= 0.4142135623730951)       r = atanSeries(x);
	else if (x <= 2.414213562373095)   r = PRIZM_PI_4 + atanSeries((x - 1.0) / (x + 1.0));
	else                               r = PRIZM_PI_2 - atanSeries(1.0 / x);
	return neg ? -r : r;
}

__attribute__((weak))
double asin(double x) {
	if (x >= 1.0) return PRIZM_PI_2;
	if (x <= -1.0) return -PRIZM_PI_2;
	double d = sqrt(1.0 - x * x);
	if (d == 0.0) return x > 0.0 ? PRIZM_PI_2 : -PRIZM_PI_2;
	return atan(x / d);
}

__attribute__((weak))
double acos(double x) { return PRIZM_PI_2 - asin(x); }

__attribute__((weak))
double atan2(double y, double x) {
	if (x > 0.0)               return atan(y / x);
	if (x < 0.0 && y >= 0.0)   return atan(y / x) + PRIZM_PI;
	if (x < 0.0)               return atan(y / x) - PRIZM_PI;
	if (y > 0.0)               return PRIZM_PI_2;
	if (y < 0.0)               return -PRIZM_PI_2;
	return 0.0;
}

__attribute__((weak))
double sinh(double x) { double e = exp(x); return 0.5 * (e - 1.0 / e); }

__attribute__((weak))
double cosh(double x) { double e = exp(x); return 0.5 * (e + 1.0 / e); }

__attribute__((weak))
double tanh(double x) {
	double e = exp(2.0 * x);
	return (e - 1.0) / (e + 1.0);
}

/* =====================================================================
   windows.h console subset
   ===================================================================== */

static WORD g_consoleAttr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

__attribute__((weak))
HANDLE GetStdHandle(DWORD nStdHandle) {
	(void)nStdHandle;
	/* A console program only ever compares this against NULL and passes it
	   back, so one non-null cookie is all that is needed. */
	return (HANDLE)(size_t)1;
}

__attribute__((weak))
BOOL GetConsoleCursorInfo(HANDLE h, CONSOLE_CURSOR_INFO* info) {
	(void)h;
	if (!info) return FALSE;
	info->dwSize = 25;
	info->bVisible = prizm_con_cursor_state();
	return TRUE;
}

__attribute__((weak))
BOOL SetConsoleCursorInfo(HANDLE h, const CONSOLE_CURSOR_INFO* info) {
	(void)h;
	if (!info) return FALSE;
	prizm_con_cursor(info->bVisible);
	return TRUE;
}

__attribute__((weak))
BOOL GetConsoleScreenBufferInfo(HANDLE h, CONSOLE_SCREEN_BUFFER_INFO* info) {
	(void)h;
	if (!info) return FALSE;
	SHORT w = (SHORT)prizm_con_cols();
	SHORT ht = (SHORT)prizm_con_rows();
	info->dwSize.X = w;
	info->dwSize.Y = ht;
	info->dwCursorPosition.X = 0;
	info->dwCursorPosition.Y = 0;
	info->wAttributes = g_consoleAttr;
	info->srWindow.Left = 0;
	info->srWindow.Top = 0;
	info->srWindow.Right = (SHORT)(w - 1);
	info->srWindow.Bottom = (SHORT)(ht - 1);
	info->dwMaximumWindowSize.X = w;
	info->dwMaximumWindowSize.Y = ht;
	return TRUE;
}

__attribute__((weak))
BOOL SetConsoleScreenBufferSize(HANDLE h, COORD size) {
	(void)h; (void)size;
	return TRUE;
}

__attribute__((weak))
BOOL SetConsoleCursorPosition(HANDLE h, COORD pos) {
	(void)h;
	prizm_con_goto(pos.X, pos.Y);
	return TRUE;
}

__attribute__((weak))
BOOL SetConsoleTextAttribute(HANDLE h, WORD attributes) {
	(void)h;
	g_consoleAttr = attributes;
	return TRUE;
}

__attribute__((weak))
BOOL FillConsoleOutputCharacterA(HANDLE h, CHAR c, DWORD count, COORD start) {
	(void)h;
	prizm_con_goto(start.X, start.Y);
	if (!c) c = ' ';
	char buf[129];
	for (int i = 0; i < 128; i++) buf[i] = c;
	buf[128] = 0;
	DWORD left = count;
	while (left > 0) {
		DWORD n = left < 128u ? left : 128u;
		buf[n] = 0;
		prizm_con_write(buf);
		buf[n] = c;
		left -= n;
	}
	return TRUE;
}

__attribute__((weak))
BOOL FillConsoleOutputAttribute(HANDLE h, WORD attr, DWORD count, COORD start) {
	(void)h; (void)attr; (void)count; (void)start;
	return TRUE;
}

__attribute__((weak))
void Sleep(DWORD milliseconds) { OS_InnerWait_ms((int)milliseconds); }

__attribute__((weak))
void ExitProcess(UINT code) { exit((int)code); }

/* =====================================================================
   failure hook for the standard library shims

   A PC standard library reports a broken precondition by throwing, and there
   are no exceptions here, so the container headers in cpplib call this instead
   of a throw.  It names the problem on the console and waits for a key, which
   is the same treatment a failed assert gets, and it is deliberately loud: an
   out of range at() is a bug in the program, not something to hide.
   ===================================================================== */

__attribute__((weak))
void prizm_stl_fatal(const char* what) {
	prizm_con_write("\nstd::*: ");
	prizm_con_write(what ? what : "failure");
	prizm_con_write("\npress a key\n");
	prizm_con_flush();
	int key = 0;
	for (;;) { GetKey(&key); if (key != 0) break; }
}

}  /* extern "C" */

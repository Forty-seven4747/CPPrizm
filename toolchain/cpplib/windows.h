#ifndef PRIZM_CPPSHIM_WINDOWS_H
#define PRIZM_CPPSHIM_WINDOWS_H

/* The small slice of <windows.h> a console program usually touches, mapped
 * onto the Prizm's on-screen console.
 *
 * A program written for a Windows console asks the OS for a handle to the
 * screen buffer, reads the window size, parks the cursor with gotoxy and
 * sleeps in milliseconds.  The Prizm has none of that, so the console layer
 * (prizm_console.cpp) keeps a virtual screen that mimics an 80x25 text window
 * and these entry points drive it.  Only the console parts exist; anything
 * else a Windows program expects (windows, messages, threads) is deliberately
 * absent, so a program that needs it fails to compile rather than silently
 * misbehaving on the calculator.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int            BOOL;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned long  DWORD;
typedef unsigned int   UINT;
typedef long           LONG;
typedef short          SHORT;
typedef char           CHAR;
typedef void*          HANDLE;
typedef const char*    LPCSTR;
typedef char*          LPSTR;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

#define STD_INPUT_HANDLE  ((DWORD)-10)
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE  ((DWORD)-12)

#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)

#define FOREGROUND_BLUE      0x0001
#define FOREGROUND_GREEN     0x0002
#define FOREGROUND_RED       0x0004
#define FOREGROUND_INTENSITY 0x0008
#define BACKGROUND_BLUE      0x0010
#define BACKGROUND_GREEN     0x0020
#define BACKGROUND_RED       0x0040
#define BACKGROUND_INTENSITY 0x0080

typedef struct _COORD {
	SHORT X;
	SHORT Y;
} COORD;

typedef struct _SMALL_RECT {
	SHORT Left;
	SHORT Top;
	SHORT Right;
	SHORT Bottom;
} SMALL_RECT;

typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
	COORD      dwSize;
	COORD      dwCursorPosition;
	WORD       wAttributes;
	SMALL_RECT srWindow;
	COORD      dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;

typedef struct _CONSOLE_CURSOR_INFO {
	DWORD dwSize;
	BOOL  bVisible;
} CONSOLE_CURSOR_INFO;

HANDLE GetStdHandle(DWORD nStdHandle);

BOOL GetConsoleCursorInfo(HANDLE h, CONSOLE_CURSOR_INFO* info);
BOOL SetConsoleCursorInfo(HANDLE h, const CONSOLE_CURSOR_INFO* info);

BOOL GetConsoleScreenBufferInfo(HANDLE h, CONSOLE_SCREEN_BUFFER_INFO* info);
BOOL SetConsoleScreenBufferSize(HANDLE h, COORD size);

BOOL SetConsoleCursorPosition(HANDLE h, COORD pos);
BOOL SetConsoleTextAttribute(HANDLE h, WORD attributes);

BOOL FillConsoleOutputCharacterA(HANDLE h, CHAR c, DWORD count, COORD start);
BOOL FillConsoleOutputAttribute(HANDLE h, WORD attr, DWORD count, COORD start);

void Sleep(DWORD milliseconds);
void ExitProcess(UINT code);

#ifdef __cplusplus
}
#endif

#endif

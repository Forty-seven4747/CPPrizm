#ifndef PRIZM_CONIO_H
#define PRIZM_CONIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int _kbhit(void);
int _getch(void);
int _getche(void);
int _getwch(void);
int _getwche(void);
int _ungetch(int c);
int _ungetwch(int c);
int _putch(int c);
int _putwch(int c);
int _cputs(const char* s);
int _putws(const wchar_t* s);
int _cprintf(const char* fmt, ...);
int _cwprintf(const wchar_t* fmt, ...);

int  prizm_getkey(void);
int  prizm_keypoll(int* matrix);
int  prizm_getraw(int* matrix);
int  prizm_key_extend(int keycode);
void prizm_kbd_flush(void);

#ifdef __cplusplus
}
#endif

#ifndef PRIZM_CONIO_NO_LEGACY
#ifdef __cplusplus
inline int getch(void)  { return _getch(); }
inline int getche(void) { return _getche(); }
inline int kbhit(void)  { return _kbhit(); }
inline int putch(int c) { return _putch(c); }
inline int ungetch(int c) { return _ungetch(c); }
#else
#define getch()   _getch()
#define getche()  _getche()
#define kbhit()   _kbhit()
#define putch(c)  _putch(c)
#define ungetch(c) _ungetch(c)
#endif
#endif

#endif

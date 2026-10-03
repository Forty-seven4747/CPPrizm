#include <stddef.h>
#include <fxcg/display.h>
#include <fxcg/heap.h>

extern "C" void (*__init_array_start[])(void);
extern "C" void (*__init_array_end[])(void);

static void prizm_run_global_ctors(void) {
	void (**ctor)(void);
	for (ctor = __init_array_start; ctor < __init_array_end; ++ctor) {
		if (*ctor) {
			(*ctor)();
		}
	}
}

extern "C" int prizm_user_main(void);

extern "C" void prizm_con_end(void) __attribute__((weak));

int main(void) {
	prizm_run_global_ctors();
	int r = prizm_user_main();
	if (prizm_con_end) prizm_con_end();
	return r;
}

void *operator new(size_t n) { return sys_malloc((int)n); }
void *operator new[](size_t n) { return sys_malloc((int)n); }
void operator delete(void *p) { if (p) sys_free(p); }
void operator delete[](void *p) { if (p) sys_free(p); }
void operator delete(void *p, size_t) { if (p) sys_free(p); }
void operator delete[](void *p, size_t) { if (p) sys_free(p); }

extern "C" void __cxa_pure_virtual(void) {}
extern "C" {
void *__dso_handle = 0;
}
extern "C" int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }

/* Function local statics of a class type normally go through these guards.
   The Makefile builds with -fno-threadsafe-statics, which is right on a single
   threaded calculator and removes the calls entirely, but a translation unit
   compiled without that flag (a hand built object, a library dropped in) would
   otherwise fail to link.  With no threads to race against, "first call runs
   the initialiser" is the whole contract. */
extern "C" int __cxa_guard_acquire(long long *guard) { return !*(char *)guard; }
extern "C" void __cxa_guard_release(long long *guard) { *(char *)guard = 1; }
extern "C" void __cxa_guard_abort(long long *guard) { (void)guard; }

void fillArea(int x, int y, int width, int height, unsigned short color) {
	unsigned short *vram = (unsigned short *)GetVRAMAddress();
	int i, j;
	for (j = 0; j < height; j++) {
		for (i = 0; i < width; i++) {
			vram[(y + j) * LCD_WIDTH_PX + x + i] = color;
		}
	}
}

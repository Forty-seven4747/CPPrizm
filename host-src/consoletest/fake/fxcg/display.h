#ifndef FAKE_FXCG_DISPLAY_H
#define FAKE_FXCG_DISPLAY_H
#ifdef __cplusplus
extern "C" {
#endif

#define LCD_WIDTH_PX 384
#define LCD_HEIGHT_PX 216

typedef unsigned short color_t;
#define COLOR_BLACK ((color_t)0x0000)
#define COLOR_BLUE ((color_t)0x001F)
#define COLOR_GREEN ((color_t)0x0400)
#define COLOR_CYAN ((color_t)0x07FF)
#define COLOR_RED ((color_t)0xF800)
#define COLOR_PURPLE ((color_t)0x8010)
#define COLOR_YELLOW ((color_t)0xFFE0)
#define COLOR_WHITE ((color_t)0xFFFF)

enum { TEXT_COLOR_BLACK = 0, TEXT_COLOR_BLUE, TEXT_COLOR_GREEN, TEXT_COLOR_CYAN,
       TEXT_COLOR_RED, TEXT_COLOR_PURPLE, TEXT_COLOR_YELLOW, TEXT_COLOR_WHITE };
enum { TEXT_MODE_NORMAL = 0x00, TEXT_MODE_INVERT = 0x01, TEXT_MODE_TRANSPARENT_BACKGROUND = 0x20 };

void PrintXY(int x, int y, const char* string, int mode, int color);
void PrintMini(int* x, int* y, const char* string, int mode_flags, unsigned int xlimit,
               int P6, int P7, int color, int back_color, int writeflag, int P11);
void PrintMiniMini(int* x, int* y, const char* string, int mode1, char color, int mode2);
void Bdisp_EnableColor(int n);
void* GetVRAMAddress(void);
void Bdisp_PutDisp_DD(void);
void Bdisp_PutDisp_DD_stripe(int y1, int y2);

#ifdef __cplusplus
}
#endif
#endif

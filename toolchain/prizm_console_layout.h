#ifndef PRIZM_CONSOLE_LAYOUT_H
#define PRIZM_CONSOLE_LAYOUT_H

/* Shared screen geometry for the on screen console.
   Included by toolchain\prizm_console.cpp and by the PC test harness, so the
   two can never drift apart.

   The Prizm screen is 384 x 216 px.  The OS keeps the top 24 px for its status
   bar and the bottom 24 px for the function key bar, so the console owns the
   168 px strip from y 24 to y 191.

   Three font schemes, all known to the builder, selected with PRIZM_CON_FONT:

     1  PrintXY        24 px font, fixed 18 x 24 cell  ->  21 cols x  7 rows
     2  PrintMini      18 px font, 12 px cell          ->  32 cols x  9 rows
     3  PrintMiniMini  10 px font,  7 px cell          ->  54 cols x 16 rows

   The 18 px and 10 px fonts are variable width, so the console draws one
   character per fixed cell to keep its columns aligned.  COL_W for scheme 3 is
   the one number worth tuning by eye: if the smallest font looks loose or the
   characters touch, adjust that single value. */

#if PRIZM_CON_FONT == 1

#  define CON_COLS 21
#  define CON_ROWS 7
#  define COL_W    18
#  define ROW_H    24

#elif PRIZM_CON_FONT == 3

#  define CON_COLS 54
#  define CON_ROWS 16
#  define COL_W    7
#  define ROW_H    10

#else

#  define CON_COLS 32
#  define CON_ROWS 9
#  define COL_W    12
#  define ROW_H    18

#endif

/* First pixel line below the status bar. */
#define ROW_TOP 24

/* Virtual height of the console.
 *
 * A console program written for Windows assumes a 25 row text window and lays
 * itself out for one: a 10 x 10 map on the first ten rows, a dialogue line on
 * row lim + 3 = 13, and so on.  The Prizm cannot show 25 readable rows, so the
 * console keeps CON_VROWS rows of text in memory and displays the top CON_ROWS
 * of them.  That is why a program with this layout has to be built with the
 * small font: only its 16 rows are tall enough to show both the map and the
 * dialogue line.  The width is not virtual, the window is as many columns wide
 * as it is drawn wide, so wrapping stays exactly where a program expects it. */
#define CON_VROWS 25

#endif /* PRIZM_CONSOLE_LAYOUT_H */

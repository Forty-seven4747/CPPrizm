Prizm C++ Add-in Builder
Build a C++ source file into a .g3a add-in for the Casio Prizm (fx-CG10 / CG20 / CG50).

In one line: pick a .cpp or a project folder, click Build, get a .g3a. The toolchain
underneath is the standard community PrizmSDK flow (sh3eb-elf-g++ and mkg3a). You do
not have to set any environment variable by hand.

Even a plain console program works. Source like

    #include <iostream>
    using namespace std;
    int main() {
        cout << "Hello world!" << endl;
        return 0;
    }

compiles as is and prints on the calculator screen. No Prizm specific code needed.

Keyboard input can be read the PC way too: #include <conio.h> gives _getch, _getwch
and _kbhit, and the SHIFT and ALPHA layers are handled by the Prizm OS itself, so
pressing ALPHA and then a letter key simply gives you that letter.

Whole multi file projects build too, not just single files: point the builder at a
folder and every .cpp, .c and .h under it is compiled together, with the file that
holds main() found automatically. See 3.5.

The PC libraries such a project normally uses are simulated as well, so code written
for a Windows console usually builds unchanged: <iostream>, std::string, <conio.h>,
<cstring>, <cmath>, <cstdlib> with rand, srand and system, <ctime> with time, and the
console part of <windows.h>. See 3.6 for what each one becomes and where the
calculator forces a difference.


1. How to use it

Graphical interface (recommended)
    Double click cpprizm-gui.exe
    - Click File... and pick a .cpp, or Folder... and pick a project folder
    - Pick the console font and the console background (see 3.4)
    - Click Build
    - The log area shows the compiler output live, and tells you where the .g3a is
    - Shortcut: drag a .cpp file, or a project folder, onto the window

Command line and batch files
    cpprizm.exe  <source.cpp | project folder>  [--log <out.txt>]
                           [--font big|medium|small] [--bg white|black]
    - Exit code 0 means success, non zero means failure
    - A bad --font or --bg value stops the build with an error, it does not quietly
      fall back to the default, so a typo in a batch file cannot go unnoticed
    - Examples:
        cpprizm.exe "D:\code\snake.cpp"
        cpprizm.exe "D:\code\snake"
        cpprizm.exe ..\src\mygame.cpp --log build.txt
        cpprizm.exe ..\src\mygame.cpp --font small --bg black

    The graphical build also runs without a window:
        cpprizm-gui.exe --build "D:\code\snake.cpp" --font small --bg black

    The two options are compiled into the .g3a, they are not a menu on the
    calculator, so build again to change them.


2. What lives where

    E:\Desktop\prizm\
    +-- cpprizm-gui.exe        graphical version, double click this
    +-- cpprizm.exe    command line version
    +-- cpprizm-zh_cn.exe  the same graphical version, kept for the CN build
    +-- README.txt               this file
    +-- examples\
    |   +-- hello_world.cpp      smallest iostream example
    |   +-- hello.cpp            bigger example: classes, templates, cin, globals
    |   +-- conio_demo.cpp       keyboard input demo
    |   +-- font_demo.cpp        the source behind out\font_demo_*.g3a, see 3.4
    +-- src\                     single file sources you build are copied here, so the
    |                            builder always has a copy next to it. Project folders
    |                            are used where they are and are not touched.
    +-- out\                     the result: .g3a, plus .bin and .map
    +-- tmp\<project>\           per build work directory, look here when a build fails
    |                            it contains src\, cppshim\, cpplib\ and the Makefile
    +-- toolchain\
    |   +-- prizm_cpp.x          C++ linker script, keeps the global ctor table
    |   +-- prizm_support.cpp    C++ runtime: constructors, new and delete
    |   +-- prizm_console.cpp    on screen console plus conio keyboard
    |   +-- prizm_rt.cpp         the simulated PC library: rand, time, maths, windows.h
    |   +-- prizm_console_layout.h   columns, rows and cell size of the three fonts
    |   +-- cppshim\             the headers a PC build can share: streams and conio
    |   |   +-- iostream         cout, cin, cerr, endl and friends
    |   |   +-- ostream          one line include of <iostream>
    |   |   +-- istream          one line include of <iostream>
    |   |   +-- conio.h          _getch, _getwch, _kbhit, Windows style
    |   +-- cpplib\              the C++ standard headers the Prizm lacks, see 3.6
    |   |   +-- string           std::string, on top of malloc
    |   |   +-- cstring cstdlib cstdio cmath ctime cstddef cctype climits cassert
    |   |   +-- cstdint windows.h
    |   +-- Makefile.template    template used to generate the Makefile
    +-- icons\                   menu icons: unselected.bmp and selected.bmp
    +-- sdk\                     full PrizmSDK-win-0.5.2 with the cross compiler and mkg3a
    +-- host-src\
        +-- cpprizm.cpp    source of this tool
        +-- consoletest\         PC side test harness for the console, no calculator needed
        +-- rebuild-host.bat     rebuild this tool, rarely needed

The sources are intentionally comment free, and all identifiers and all output text
are plain ASCII English.


3. What you can write in your source

Supported, verified on real hardware
    - Classes, inheritance, virtual and pure virtual functions, dynamic dispatch
    - Function templates and class templates
    - new, delete, new[], delete[] on top of sys_malloc
    - Global objects and static members, their constructors run before main
    - Function local statics
    - The whole printf family including width, precision, flags, %f, %g, %lld, %p
    - #include <iostream> with cout and cin, write it like a normal console program
    - std::string, hand written because there is no libstdc++, see 3.6
    - A whole project folder in one build, with the file holding main() found for you,
      see 3.5
    - The simulated PC library in 3.6: rand, srand, time, system("cls"), the maths
      functions and the Windows console calls
    - A large part of the C++ standard library: the STL containers (vector, deque,
      list, stack, queue, priority_queue, map, multimap, set, multiset, unordered_map,
      unordered_multimap, unordered_set, unordered_multiset, array, bitset), string,
      string_view, tuple, optional, pair, the iterators, the full <algorithm> and
      <numeric>, smart pointers (unique_ptr, shared_ptr, weak_ptr), functional,
      type_traits, <sstream>, <iomanip>, <random>, <complex>, <chrono>, <atomic>,
      <mutex>, <cwchar> and more, see 3.6

Not supported
    - Exceptions, try, catch, throw, the build uses -fno-exceptions. Where the
      standard would throw, the runtime prints and stops instead
    - RTTI, typeid and dynamic_cast, the build uses -fno-rtti
    - Threads, files, sockets and anything else that needs an operating system.
      <mutex> and <atomic> exist with the standard interface and are no-ops,
      <fstream> compiles but open() always fails, see 3.6


3.1 How far iostream goes

There is no libstdc++ on the Prizm, so <iostream> is a small home made version.
The goal is that you can write an ordinary console program exactly like on a PC.
These all work:

    cout << number / string / char / double / pointer
    cout << endl, flush, dec, hex, oct, fixed, scientific, boolalpha, ...
    cout << setw(n), setfill(c), setprecision(p), setbase(b)
    cout << showpos, showbase, uppercase, left, right
    cin >> int, long long, unsigned, double, char, char[]
    if (cin >> n) { ... }                usable as a condition
    cin.get(), cin.get(ch), cin.getline(buf, n), cin.ignore(n)
    cin.fail(), cin.clear()

The catch is that the Prizm has no terminal and no files, only a character screen,
so cout and cin work by drawing on the calculator display:

    - The screen is 32 columns by 9 rows, drawn on the strip between the 24 px
      status bar on top and the 24 px function key bar at the bottom
    - Writing past the right edge wraps, writing past row 9 scrolls up
    - '\b' backspace, '\t' tab and '\n' newline all work
    - For input, cin waits for keys on the calculator, EXE confirms the line and
      AC cancels, which makes cin.fail() return 1
    - If the last line is not terminated when the program ends, the runtime adds a
      "-- Press any key --" line and waits, so the output does not flash by

So this snippet compiles and runs directly, and the screen shows Hello world!
examples\hello_world.cpp is exactly that code, ready to build.

<iostream> and <stdio.h> can be mixed, printf output and cout output share the same
screen because printf is redirected there as well.

Limits, important:
    - Only ASCII is printable. Non ASCII text shows as boxes because all three OS
      text fonts are ASCII only.
    - There is no std::string. Use char[] or const char*.
    - Touch, graphics and colour drawing still need libfxcg: PrintXY, Bdisp_*.


3.2 Keyboard input: conio.h

To read keys the PC way:

    #include <conio.h>

    while (!_kbhit()) { }        non blocking, returns as soon as a key waits
    int c = _getch();            take one key, without echo
    int w = _getwch();           wide character version

Available functions:

    _kbhit()     is a key waiting, does not block
    _getch()     take one key, no echo
    _getche()    take one key, with echo
    _getwch()    wide version of _getche, extended keys are prefixed with 0xE0
    _getwche()
    _ungetch(c)  push a character back
    _putch(c),   _cputs(s),   _cprintf(fmt, ...)    write to the screen
    _putwch(c),  _putws(s),   _cwprintf(fmt, ...)
    getch, kbhit, putch, cputs and ungetch also exist as classic aliases

Return values, aligned with Windows conio:

    normal character    the character itself, ASCII 0x20 to 0x7E
    EXE                 carriage return 13
    DEL                 backspace 8
    EXIT                escape 27
    AC                  Ctrl-C 3
    arrows, F1 to F6    first call returns 0, _getwch returns 0xE0,
                        the second call returns the extended code
                        UP=72  DOWN=80  LEFT=75  RIGHT=77  F1..F6=59..64
    other calculator keys, including sin, log, x squared, are skipped

Calculator operator keys are not ASCII, conio folds them onto PC characters:

    +  -> '+'      -  -> '-'      (-) -> '-'     x  -> '*'     /  -> '/'
    a b/c -> '/'   ^  -> '^'      EXP -> 'E'

SHIFT and ALPHA are not characters, they are shift states, so conio never returns
them and _getch waits for the next real key. The layer itself is translated by the
Prizm OS: press ALPHA, then X,theta,T and you get 'A'. That is why letters work
without conio knowing anything about the layer tables.

Note on MENU: GetKey handles it, so pressing MENU opens the main menu instead of
returning a key. Use EXIT when you want escape 27.

Extra calculator level functions:

    prizm_getkey()          blocking, returns the raw OS key code with modifiers
                            already applied, including sin, log and friends
    prizm_getraw(&mat)      blocking, returns the raw matrix code 0xCCRR where
                            CC is the column 7..2 left to right and RR is the row
                            2..10 bottom to top, AC/ON is the special case 0x0101
    prizm_keypoll(&mat)     non blocking version of prizm_getraw
    prizm_key_extend(k)     OS key code to Windows extended code, 0 when it is not
                            an extended key
    prizm_kbd_flush()       drop everything that is still queued


3.3 Source skeleton

Write the entry point normally as int main(void).
The builder adds -Dmain=prizm_user_main to that one file, so main is renamed, the
C++ runtime runs the global constructors first and then calls your code. You do not
have to do anything for that.

Plain iostream version, good for exercises:

    #include <iostream>
    using namespace std;
    int main() {
        int a, b;
        cout << "a b = ";
        cin >> a >> b;
        cout << a << " + " << b << " = " << (a + b) << endl;
        return 0;
    }

libfxcg graphics version, good for games and custom drawing:

    #include <fxcg/display.h>
    #include <fxcg/keyboard.h>

    int main(void) {
        Bdisp_EnableColor(1);
        Bdisp_AllClr_VRAM();
        /* PrintXY eats the first two bytes of the string, hence the xx */
        PrintXY(1, 1, (char*)"xxHello, Prizm!", TEXT_MODE_NORMAL, TEXT_COLOR_BLUE);
        Bdisp_PutDisp_DD();
        int key = 0;
        while (1) { GetKey(&key); if (key == KEY_CTRL_EXIT) break; }
        return 0;
    }

Common libfxcg headers:

    #include <fxcg/display.h>    Bdisp_*, PrintXY, GetVRAMAddress, colour constants
    #include <fxcg/keyboard.h>   GetKey, KEY_CTRL_EXIT and friends
    #include <fxcg/heap.h>       sys_malloc, sys_free
    #include <stdio.h>           printf, sprintf


3.4 Console font and background

The console can be drawn with three different OS text fonts, and on either a white
or a black background. Pick both in the builder, they are compiled into the .g3a, so
build again to change them.

In cpprizm-gui.exe both are two rows of radio buttons above the Build button. On
the command line and in batch files use the two switches:

    cpprizm.exe game.cpp --font medium --bg white
    cpprizm.exe game.cpp --font small  --bg black

    --font big      PrintXY, the 24 px font        21 columns by  7 rows
    --font medium   PrintMini, the 18 px font      32 columns by  9 rows   (default)
    --font small    PrintMiniMini, the 10 px font  54 columns by 16 rows

    --bg white      white paper, dark ink   (default)
    --bg black      black paper, light ink

big prints the fewest characters but is the most readable, small fits the most and
is meant for dense output such as tables or dumps. The columns and rows shown are
what fits between the status bar and the function key bar; the console scrolls when
you run past them, exactly as before.

Three notes:

    - textcolor() and the palette index arguments keep working in both backgrounds.
      When the paper is black the builder swaps the ink and paper roles for you, so
      TEXT_COLOR_BLACK really does mean black on the white parts.
    - The small font is the exception to that. PrintMiniMini takes no background
      argument, and the OS paints its own default background, which is white,
      behind every glyph. On a black paper that turned every cell into a white
      block, so the console asks for a black background through the mode bits
      instead. That also pins the glyphs to white, which means textcolor() cannot
      choose a colour for the small font on black paper: it is black and white
      only. The other five combinations are unaffected.
    - A background of black fills the whole console area with black first, so the
      strip is uniformly dark instead of only the glyphs being drawn.

Six ready made samples sit in out\, one per combination, so you can compare them on
the calculator before deciding:

    out\font_demo_big_white.g3a     out\font_demo_big_black.g3a
    out\font_demo_med_white.g3a     out\font_demo_med_black.g3a
    out\font_demo_small_white.g3a   out\font_demo_small_black.g3a

Each one prints the alphabet, the digits and a few lines of text, which is enough to
see how much fits and how readable each font is. They are ordinary add-ins, flash the
one you like with the normal link software or delete them once you have decided.
examples\font_demo.cpp is the source they were built from, so you can rebuild any of
them yourself after changing the console.

How it is wired, in case you edit the console:

    toolchain\prizm_console_layout.h   the geometry of all three schemes
    toolchain\prizm_console.cpp        the console itself, reads the two macros
    tmp\<project>\src\prizm_console_cfg.h   written per build by the builder

The builder drops a small generated header, prizm_console_cfg.h, into the project
before compiling it. prizm_console.cpp includes it when present and falls back to
medium on white when it is not, which is why one console source serves all six
combinations without being edited. See the FAQ entry "Changing how the console
behaves" if you want to add a font size of your own.


3.5 Multi file projects

Point the builder at a folder instead of at a file and the whole project is built:

    cpprizm.exe "D:\code\mygame" --font small --bg white

    - Every .cpp, .c, .h, .hpp, .inl, .ipp and .s file under the folder is found, at
      any depth, and copied into one flat src\ directory in the work area
    - The file that defines main() is located by reading the sources, and it is
      renamed to <foldername>.cpp. That name matters: it is the one file the Makefile
      treats specially, so the entry point ends up with the right symbol
    - Your folder is never modified. Everything happens in tmp\<foldername>\ and the
      result lands in out\<foldername>.g3a, with .bin and .map beside it

What it refuses, and why:

    - Two source files with the same base name in different subfolders. The flat
      src\ layout cannot tell them apart, so rename one. It says which two.
    - A project that already contains <foldername>.cpp while another file defines
      main, because the two would collide over the same name. It says so rather than
      silently picking one.
    - If no file defines main(), it warns and compiles the first source anyway

A Dev-C++ .dev file, a .vcxproj or a Makefile of your own is not needed and is simply
not copied, because the builder writes its own Makefile. Anything else it cannot
compile is left behind too, so having a few text files in the folder is fine.


3.6 The PC libraries, and what they become

The SDK ships a C library and libfxcg, but no libstdc++ and no libm. The builder
closes that gap so code written for a Windows console usually builds unchanged.

    <iostream> <istream> <ostream>   cppshim\, plus prizm_console.cpp and prizm_rt.cpp
    <string>                         cpplib\string, a std::string on top of malloc
    <conio.h>                        cppshim\conio.h, keys taken from the OS keyboard
    <cstring>                        the SDK string.h, re-exported into namespace std
    <cstdio>                         the console printf family, plus snprintf and fputs
    <cstdlib>                        SDK stdlib, plus system("cls"), rand and srand
    <ctime>                          time, difftime, localtime, gmtime
    <cmath>                          hand written sqrt, ceil, sin, cos, tan and the rest
    <cstddef> <cctype> <climits> <cstdint> <cassert>    the missing C++ wrappers
    <windows.h>                      the console part, mapped onto the text window

Where the calculator forces a difference:

    rand, srand     sys_rand and sys_srand. The sequence is fixed and the seed has
                    little effect, so srand(time(0)) is harmless but does not make a
                    run unpredictable
    time(0)         seconds since midnight, read from the real time of day clock. This
                    SDK has no calendar call, so it is not a Unix timestamp: two runs
                    starting in the same second of the day get the same value, and
                    localtime fills in a fixed date rather than today's. Good enough
                    for srand(time(0)) and for "how long did that take"
    system("cls")   clears the console, and it is case insensitive. Any other command,
                    including the system("pause") that litters Windows console code,
                    does nothing at all and reports success, so a ported program does
                    not stall waiting for a key press that will never be interpreted
    Sleep(ms)       OS_InnerWait_ms. The console does not respond during the wait,
                    which matches a PC, so an animation loop keeps its timing
    gotoxy          SetConsoleCursorPosition, moves the console cursor
    getConsoleRows, getConsoleLength    GetConsoleScreenBufferInfo, see below
    hidecursor, showcursor              Get/SetConsoleCursorInfo
    GetStdHandle    returns a non null cookie. Programs only ever compare it against
                    NULL and hand it back, so that is all it has to be
    FillConsoleOutputCharacterA    fills with one character, the usual way to blank a
                    line
    textcolor, SetConsoleTextAttribute    sets the ink colour
    ExitProcess     becomes exit()

The maths, all in prizm_rt.cpp, all weak so a real implementation wins if one ever
turns up:

    fabs labs floor ceil trunc round frexp ldexp modf fmod sqrt
    exp log log10 pow sin cos tan asin acos atan atan2 sinh cosh tanh

How tall the console is

A console program often asks how big it is, or simply assumes it, and that is where
the calculator and a PC differ most. GetConsoleScreenBufferInfo reports:

    width   what the chosen font fits, 21, 32 or 54 columns, see 3.4
    height  25, always

25 is the height of a plain Windows text window, and it is what a program written for
one expects: a 10 by 10 map on rows 0 to 9 and a line of dialogue at row 13, say. So
the console keeps 25 rows of text in memory whatever the font. It draws only the rows
that fit between the status bar and the function key bar, and the rest scroll in as
usual.

That is why a program with an old 80 by 25 layout wants the small font. The strip is
168 px tall, so the big font shows 7 of those 25 rows and the medium font shows 9,
neither of which can reach the dialogue line at 13. The small font shows 16 rows,
which is the only scheme that fits both the map and the dialogue.

The width is not virtual. A program that writes at column 40 cannot be shown on a
21 column screen, and the console will not pretend otherwise; it wraps. Pick the small
font, which gives 54 columns, or change the program.

See the FAQ entry "Written for an 80 by 25 console" for what to do when an old
project still looks wrong after that.


4. Installing the .g3a on the calculator

    1. Send out\xxx.g3a to the calculator with the official link software such as
       FA-124 or FA-CG
    2. Or copy the .g3a to calculator storage by hand and move or copy it into main
       memory with Memory Manager on the calculator
    3. It then appears in the Main Menu, with the icon from icons\


5. Why the extra files in toolchain are needed

The PrizmSDK startup code jumps straight into main() and it:

    - never runs the C++ global constructor array, so global objects would be garbage
    - provides no operator new or delete
    - has no libstdc++ and no libm, not even sin or sqrt

So the builder links an extra prizm_support.cpp into every project, which does:

    1. Defines the real entry main(). It walks __init_array_start to __init_array_end,
       calls every global constructor, then calls your prizm_user_main()
    2. Implements operator new, delete, new[] and delete[] on top of sys_malloc
    3. Provides the ABI symbols GCC expects: __cxa_pure_virtual, __dso_handle and
       __cxa_atexit
    4. Provides fillArea(), a small helper the old Casio SDK had and libfxcg does not
    5. Calls prizm_con_end() after main returns, when the console was used, to add
       the closing prompt line

iostream, printf and conio live in prizm_console.cpp:

    - The <iostream> header in cppshim\ declares std::cout, std::cin and the
      operator<< and operator>> overloads
    - prizm_console.cpp keeps a character buffer whose size comes from the chosen
      font, 32 by 9 for the default. Every written character updates the buffer and
      only the changed row is redrawn with the matching OS print call
    - It also implements the whole printf, sprintf, puts and putchar family and marks
      them weak, so it backs up code that does not know libstdc++ without clashing
      with the printf.o shipped in the SDK. Whichever is referenced wins.
    - Double to text, integer to hex and pointer formatting are hand written because
      that part of libc does not exist on the Prizm
    - Those overrides are for the calculator only, and they are wrapped in
      #if defined(__sh__) for a reason. sh3eb-elf-g++ defines __sh__; a PC compiler
      does not. Without that guard the host test harness in host-src\consoletest would
      link this very file's fputs and vsnprintf, its own report would go to the
      simulated screen instead of the terminal, and a green run would look empty. If
      you port this toolchain elsewhere, keep the guard.

Keyboard, in conio.h and prizm_console.cpp:

    - _kbhit() uses GetKeyWait_OS() with KEYWAIT_HALTOFF_TIMEROFF, which returns
      immediately, so it is non blocking
    - _getch() uses GetKey(), which is the OS keyboard routine. GetKey already applies
      the SHIFT and ALPHA layers, so letters come out of the OS, and it also handles
      MENU, capture and the power off timer for you
    - The queue holds eight entries, so a character read by _kbhit() is never lost
    - Calculator operator codes, which are not ASCII, are folded onto + - * / ^ E
    - Arrow keys and F1..F6 are converted to the two call Windows extended sequence

The other half of the simulated PC library lives in prizm_rt.cpp, and everything in it
is weak, so a program that brings its own version of a function still wins:

    - rand, srand, system, time, difftime, localtime, gmtime
    - the maths functions listed in 3.6, which the SDK does not have at all
    - the <windows.h> console calls, each one mapped onto a console routine, see 3.6

Splitting it that way keeps prizm_console.cpp about the screen and the keyboard, and
prizm_rt.cpp about everything a PC program expects to find but the Prizm does not have.
A program only pulls in what it references, because the link uses --gc-sections.

Why three fonts and not just PrintXY: the OS has three text fonts an add-in can use,
and they trade size against how much fits.

    PrintXY       the 24 px font, a fixed 18 x 24 px cell, palette colour index
    PrintMini     the 18 px font, variable width, real RGB565 colours
    PrintMiniMini the 10 px font, variable width, palette colour index

PrintXY looks the best but its fixed 24 px cell only fits 21 columns by 7 rows, so
long output wraps constantly. PrintMini is the middle size and the default, 32
columns by 9 rows, and it looks far lighter. PrintMiniMini fits 54 columns by 16
rows, which is four times the text of PrintXY, and it is the one to pick for tables.
All three are ASCII only.

Things to know about the two variable width fonts:

    - PrintMini takes real colours (RGB565), not a TEXT_COLOR_* palette index the way
      PrintXY and PrintMiniMini do. prizm_console.cpp keeps a small table that maps
      the palette index onto the matching colour, so textcolor() still works
    - PrintMini takes pixel coordinates, not character cells, and mode_flags 0x40
      means "draw at exactly y, do not offset by the 24 px status bar". PrintMiniMini
      wants the same 0x40 bit, but in its mode argument
    - PrintMiniMini uses simulate != 0 to mean "measure only", which is the opposite
      of PrintMini's writeflag == 0 convention. Easy to get backwards
    - PrintMiniMini also has no back_color argument, and its default background is
      white, so on a black paper its mode has to ask for a black back colour
      explicitly (bit 2) and bit 7 has to stop it repainting over the paper the
      console already filled in. See 3.4 and the FAQ entry below

The 18 px and 10 px fonts are variable width, so drawing a whole string would give
ragged columns and break everything that lines up text. The console therefore draws
one character per fixed cell, 12 px for the medium font and 7 px for the small one,
which keeps the console monospaced. prizm_console.cpp handles all of that, so your
cout code does not have to care which font is selected.

The runtime alone is not enough, because the linker drops the whole constructor
table when nothing seems to reference it. That is why prizm_cpp.x keeps .init_array
and .ctors with KEEP() and marks the boundaries with ___init_array_start and
___init_array_end. This toolchain prefixes C symbols with an underscore, so both
spellings are defined.

Also note that this toolchain is big endian, -mb with sh3eb, so the function pointers
stored in .init_array appear byte reversed compared to a normal x86 disassembly.


6. FAQ

Q: "sprintf was not declared in this scope"
A: Add #include <stdio.h>, the C header from the SDK, not <cstdio>.

Q: undefined reference to std::string or std::vector
A: std::string works, it is provided by cpplib\string, see 3.6, so check that the
   include is resolving to this toolchain and not to a copy of your own. std::vector,
   std::map, std::list and the rest of the STL containers also work now, they live in
   cpplib\, see the table in 3.6. If one is still missing, the include may be pulling a
   different header tree. std::cout and std::cin are supported.

Q: I used <iostream> but cout prints nothing
A: Check two things. First, make sure the output ends with endl or '\n', otherwise
   the last line is only drawn when the program exits. Second, do not define another
   entry point, prizm_con_end must be called. Normally the program waits for a key
   press before it exits.

Q: Non ASCII output shows as boxes
A: That is expected. All three OS text fonts are ASCII only. To draw other
   glyphs you have to supply your own bitmap font and draw it as graphics.

Q: Pressing SHIFT or ALPHA gives nothing from _getch()
A: Correct, they are shift states, not characters. conio skips them and only returns
   the shifted result. Use prizm_keypoll() or prizm_getraw() if you want the raw
   matrix code, SHIFT is 0x0709 and ALPHA is 0x0708.

Q: Pressing sin, log or x squared gives nothing from _getch()
A: Those keys are not ASCII, so conio skips them. Use prizm_getkey() for the full
   calculator key code, for example sin is 0x81 and x squared is 0x8B.

Q: Letters do not come out, or the wrong character comes out
A: Letters are produced by the Prizm OS, so they follow the native behaviour: press
   ALPHA, release it, then press the letter key. If letters still do not appear, check
   that the add-in actually runs and that _kbhit() reports keys.

Q: _kbhit() always returns 0, or always returns 1
A: Always 0 means GetKeyWait_OS did not report an event on this machine, so use
   _getch() directly, the blocking version still works. Always 1 usually means the
   key queue was not emptied, call prizm_kbd_flush() first.

Q: After using conio, cin >> no longer receives keys
A: Both cin and conio read through GetKey, so do not mix the two styles in one
   program. Pick one.

Q: Is there sin, cos or sqrt
A: Yes. The Prizm has no libm, so prizm_rt.cpp carries hand written versions: sin cos
   tan asin acos atan atan2 sinh cosh tanh exp log log10 pow sqrt ceil floor trunc
   round fabs labs frexp ldexp modf fmod. They are good enough for a game or a plot, but
   they are not printing quality, so expect the last digit or two to differ from a PC.
   printf with %f works as well, that one is hand written too and does not need libm.

Q: "::main must return int"
A: The return type of main must be int, void main() is not allowed.

Q: The calculator reports SYSTEM ERROR right after the program finishes
A: This is the classic entry point trap, and it is invisible in the source. If the
   user main is renamed to prizm_user_main with -Dmain=prizm_user_main, the compiler
   no longer treats the function as main(), so it no longer adds the implicit
   return 0. Falling off the end of a non-void function is undefined behaviour, and
   at -Os the compiler then deletes the whole return path: no
   "lds.l @r15+,pr", no "rts". Execution runs off the end straight into the literal
   pool, hits an illegal instruction and the OS reports SYSTEM ERROR -- always at
   the exact moment the program finishes, never before.

   Programs that do write return 0; are not affected, which is why some of the
   examples worked and others did not.

   The toolchain no longer uses -Dmain=...: your source is compiled as a real
   main() and the symbol is renamed afterwards, so the implicit return 0 is kept.
   Just rebuild the .g3a. If you ever build by hand, do it the same way:

       sh3eb-elf-g++ $(CXXFLAGS) -c x.cpp -o x.o.pre
       sh3eb-elf-objcopy --redefine-sym _main=_prizm_user_main x.o.pre x.o

   and make prizm_support.cpp declare it as extern "C" int prizm_user_main(void);
   To check a build, look at prizm_user_main in <project>.map: the code must end
   with "mov #0,r0 / lds.l @r15+,pr / rts", not with a fall through into the
   literal pool.

Q: Fields of a global object look like garbage
A: Normally this cannot happen. If it does, prizm_cpp.x was not used. Check for
   -T../prizm_cpp.x in tmp\<project>\Makefile.

Q: multiple definition of printf
A: This should not happen. The printf family in prizm_console.cpp is weak and only
   takes effect when the SDK does not provide one. If it does happen, look at the
   full build log in tmp\<project>\ and check the link command line.

Q: My file name is not ASCII and the output is called addin_XXXXXX
A: The make shipped with PrizmSDK cannot handle non ASCII paths, so the builder turns
   the project name into ASCII and appends a short hash to avoid collisions. The
   output still lands in out\.

Q: The small font on a black background shows white blocks instead of text
A: That was a real bug, fixed in the current console. PrintMiniMini, the call the
   small font uses, has no back_color argument the way PrintMini has, and its
   default background is white, so on a black paper every glyph cell was painted
   white and the letters were white too, which looks like solid blocks. The fix is
   in toolchain\prizm_console.cpp: on a black paper that call now passes mode bits
   for a black back colour and for leaving the console's own fill alone. If you
   built your .g3a before the fix, just build it again, the console is copied in
   fresh on every build.

Q: Written for an 80 by 25 console and only the top few rows show
A: The console holds 25 rows, see 3.6, and how many of them are visible depends on the
   font: 7 for big, 9 for medium, 16 for small. A program written for a plain Windows
   text window puts things anywhere on rows 0 to 24. If your layout uses anything past
   row 6, or the bottom of the window, build with --font small, the only scheme that
   shows 16 rows. The width is not virtual either: a program that assumes 80 columns has
   to be changed, or moved to the small font, which gives 54.

Q: system("pause") does nothing and the program runs straight on
A: Correct, and deliberate. Only system("cls") is understood, because ported Windows
   console code uses it constantly to clear the screen. Everything else, pause included,
   returns success without doing anything, so a batch style program does not stall on a
   key press. Delete the pause calls when you port, or replace them with _getch().

Q: srand(time(0)) gives the same game every time
A: Expected on this hardware. time(0) is seconds since midnight and the seed goes to
   the system RNG, see 3.6, so two runs started in the same second begin identically, and
   the system RNG does not have the range a PC has either. Mix something else into the
   seed if you want more variety, for example a value from prizm_getkey().

Q: A project folder build is refused over two files with the same name
A: The builder flattens the project into one src\ directory, so a\util.cpp and
   b\util.cpp cannot coexist. The message names both files: rename one, or merge them.
   The same happens if the folder already contains <foldername>.cpp while another file
   defines main, because both want the name the entry point has to take.

Q: I want to check the console without flashing the calculator every time
A: Run host-src\consoletest\run-test.bat. It compiles the real
   toolchain\prizm_console.cpp against fake fxcg headers on the PC, builds all six font
   and background combinations, runs each one and prints a pass / fail count. It needs
   MinGW-w64 g++. This is the gate the console is developed against, so a change that
   breaks the layout shows up as a failure count rather than as a wrong looking screen
   on the calculator.

Q: Changing the menu icon
A: Replace icons\unselected.bmp and icons\selected.bmp. They must be 30 by 30 BMP
   files, other sizes make mkg3a fail.

Q: Changing the tool itself
A: Edit host-src\cpprizm.cpp and run host-src\rebuild-host.bat. It needs
   MinGW-w64 g++.

Q: Changing how the console behaves, for example the font size or the colours
A: The three stock schemes are already selectable, see 3.4, so first check whether
   one of them is good enough. To change geometry or add a fourth scheme, edit
   toolchain\prizm_console_layout.h, which holds CON_COLS, CON_ROWS, COL_W, ROW_H
   and ROW_TOP for every scheme, and the drawing code in toolchain\prizm_console.cpp.
   You do not have to rebuild the builder, the next build of your .cpp picks it up.

   The geometry constants have to stay consistent with the OS font:
   ROW_H is the line height of the font used, 24 for PrintXY, 18 for PrintMini and
   10 for PrintMiniMini. COL_W is the monospace cell the character is squeezed into,
   18, 12 and 7 to match. ROW_TOP 24 skips the status bar, and CON_ROWS should not
   exceed (192 - ROW_TOP) / ROW_H, because the function key bar owns the last 24 px.

   A new scheme is three steps: add a branch to prizm_console_layout.h, add the
   matching print call in hwDrawRow in prizm_console.cpp, and add the radio button
   plus a ParseFontOpt case in host-src\cpprizm.cpp, then rebuild the builder
   with rebuild-host.bat.


7. Credits

    - PrizmSDK 0.5.2, the community SDK with the sh3eb-elf cross toolchain and mkg3a
    - libfxcg, the hardware abstraction library for the Prizm
    - Compiler flags and Makefile skeleton follow community practice:
      -Os -Wall -mb -m4a-nofpu -mhitachi -nostdlib -ffunction-sections
      -fdata-sections -fno-exceptions -fno-rtti
      linked with -lc -lfxcg -lgcc and packed into a .g3a by mkg3a
    - Convert program by Forty-seven
    - Program UI design by Deepseek-V4.1-Flash

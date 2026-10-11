/* Console font comparison sample.
   Build it once per combination, the font and the paper are chosen in the
   builder and baked into the .g3a:

       cpprizm.exe examples\font_demo.cpp --font small --bg black

   The screen shows the alphabet, the digits and a few lines of ordinary
   output, which is enough to see how much fits and how readable each of the
   three fonts is. */
#include <iostream>
using namespace std;

int main() {
    cout << "Prizm console demo" << endl;
    cout << "big medium small, pick one" << endl;
    cout << "ABCDEFGHIJKLMNOPQRSTUVWXYZ" << endl;
    cout << "abcdefghijklmnopqrstuvwxyz" << endl;
    cout << "0123456789 !@#$%^&*()" << endl;
    cout << "n = 123   pi = 3.14159" << endl;
    for (int i = 1; i <= 6; i++)
        cout << "line " << i << endl;
    return 0;
}

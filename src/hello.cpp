#include <iostream>
using namespace std;

class BootCounter {
public:
	BootCounter() : m_count(1000) {}
	int tick() { return ++m_count; }
	int value() const { return m_count; }
private:
	int m_count;
};

static BootCounter g_boot;

struct Particle {
	virtual ~Particle() {}
	virtual const char* kind() const = 0;
};

struct RedBox : Particle {
	const char* kind() const { return "RedBox"; }
};

struct CyanBall : Particle {
	const char* kind() const { return "CyanBall"; }
};

template <typename T>
static T tmax(T a, T b) { return a > b ? a : b; }

template <typename T>
static T tclamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

int main() {
	cout << "=== C++ on Casio Prizm ===" << endl;
	cout << "global ctor : " << (g_boot.value() == 1000 ? "OK" : "FAILED") << endl;

	Particle* a = new RedBox();
	Particle* b = new CyanBall();
	cout << "virtual     : " << a->kind() << " / " << b->kind() << endl;
	delete a;
	delete b;

	cout << "template    : max(3,7)=" << tmax(3, 7)
	     << " clamp(99,0,50)=" << tclamp(99, 0, 50) << endl;

	cout << "types       : int=" << -42 << " double=" << 3.14159 << endl;

	static int s_calls = 0;
	s_calls++;
	cout << "static local: " << s_calls << endl;

	cout << "Type a number: ";
	int n = 0;
	if (cin >> n) {
		cout << "You typed " << n << ", x2 = " << (n * 2) << endl;
	} else {
		cout << "(cancelled)" << endl;
	}

	cout << "Done. boot ticks=" << g_boot.tick() << endl;
	return 0;
}

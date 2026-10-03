#include <iostream>
#include <stdio.h>
using namespace std;

class Vec2 {
public:
	Vec2(int x = 0, int y = 0) : m_x(x), m_y(y) {}
	Vec2 operator+(const Vec2& o) const { return Vec2(m_x + o.m_x, m_y + o.m_y); }
	int x() const { return m_x; }
	int y() const { return m_y; }
private:
	int m_x, m_y;
};

static Vec2 g_origin(1, 2);

int main() {
	char buf[64];
	sprintf(buf, "sprintf -> %d %s", 7, "ok");
	cout << buf << endl;

	printf("printf  -> %d %s\n", 8, "ok");

	Vec2 v = g_origin + Vec2(10, 20);
	cout << "Vec2    -> " << v.x() << "," << v.y() << endl;

	double d = 2.5;
	cout << "double  -> " << d << " * 4 = " << (d * 4) << endl;

	return 0;
}

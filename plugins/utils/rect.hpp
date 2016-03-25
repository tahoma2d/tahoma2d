#ifndef TOONZ_PLUGIN_HELPER_UTILS_RECT_HPP__
#define TOONZ_PLUGIN_HELPER_UTILS_RECT_HPP__

#include <algorithm>
#include <cmath>
#include <cfloat>

class ToonzRect
{
public:
	double x0, y0;
	double x1, y1;

	ToonzRect()
		: x0(0), y0(0), x1(0), y1(0) {}
	ToonzRect(double x0, double y0, double x1, double y1)
		: x0(x0), y0(y0), x1(x1), y1(y1) {}
	ToonzRect(const ToonzRect &toonzRect)
		: x0(toonzRect.x0), y0(toonzRect.y0), x1(toonzRect.x1), y1(toonzRect.y1) {}

	bool equals(double a, double b, double err = DBL_EPSILON) const
	{
		return fabs(a - b) < err;
	}

	ToonzRect operator+(const ToonzRect &toonzRect) const;
	ToonzRect operator*(const ToonzRect &toonzRect) const;
	ToonzRect &operator+=(const ToonzRect &toonzRect);
	ToonzRect &operator*=(const ToonzRect &toonzRect);
	bool operator==(const ToonzRect &toonzRect) const;

	bool isEmpty() const;
	bool isContained(const ToonzRect &toonzRect) const;
	bool isOverlapped(const ToonzRect &toonzRect) const;
	ToonzRect enlarge(double x, double y) const;
};

ToonzRect ToonzRect::operator+(const ToonzRect &toonzRect) const
{
	if (isEmpty()) {
		return toonzRect;
	} else if (toonzRect.isEmpty()) {
		return *this;
	}
	return ToonzRect(std::min(x0, toonzRect.x0), std::min(y0, toonzRect.y0),
					 std::max(x1, toonzRect.x1), std::max(y1, toonzRect.y1));
}

ToonzRect ToonzRect::operator*(const ToonzRect &toonzRect) const
{
	if (isEmpty() ||
		toonzRect.isEmpty() ||
		toonzRect.x1 < x0 ||
		x1 < toonzRect.x0 ||
		toonzRect.y1 < y0 ||
		y1 < toonzRect.y0) {
		return ToonzRect();
	}

	return ToonzRect(std::max(x0, toonzRect.x0), std::max(y0, toonzRect.y0),
					 std::min(x1, toonzRect.x1), std::min(y1, toonzRect.y1));
}

ToonzRect &ToonzRect::operator+=(const ToonzRect &toonzRect)
{
	return *this = *this + toonzRect;
}

ToonzRect &ToonzRect::operator*=(const ToonzRect &toonzRect)
{
	return *this = *this * toonzRect;
}

bool ToonzRect::operator==(const ToonzRect &toonzRect) const
{
	return equals(x0, toonzRect.x0) &&
		   equals(y0, toonzRect.y0) &&
		   equals(x1, toonzRect.x1) &&
		   equals(y1, toonzRect.y1);
}

bool ToonzRect::isEmpty() const
{
	if ((equals(x0, x1) && equals(y0, y1)) ||
		x0 > x1 ||
		y0 > y1) {
		return true;
	}
	return false;
}

bool ToonzRect::isContained(const ToonzRect &toonzRect) const
{
	return toonzRect.x0 <= x0 && x1 <= toonzRect.x1 &&
		   toonzRect.y0 <= y0 && y1 <= toonzRect.y1;
}

bool ToonzRect::isOverlapped(const ToonzRect &toonzRect) const
{
	return x0 <= toonzRect.x1 && x1 >= toonzRect.x0 &&
		   y0 <= toonzRect.y1 && y1 >= toonzRect.y0;
}

ToonzRect ToonzRect::enlarge(double x, double y) const
{
	if (isEmpty()) {
		return *this;
	}
	return ToonzRect(x0 - x, y0 - y, x1 + x, y1 + y);
}

class ToonzPoint
{
public:
	double x, y;
	ToonzPoint() : x(0), y(0) {}
	ToonzPoint(double x, double y) : x(x), y(y) {}
};

#endif

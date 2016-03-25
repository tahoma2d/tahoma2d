#ifndef TOONZ_PLUGIN_HELPER_UTILS_AFFINE_HPP__
#define TOONZ_PLUGIN_HELPER_UTILS_AFFINE_HPP__

#include <cmath>
#include <cfloat>
#include <cassert>
#include <toonz_hostif.h>
#include "rect.hpp"

class ToonzAffine
{
public:
	double a11, a12, a13;
	double a21, a22, a23;

	ToonzAffine() : a11(1), a12(0), a13(0), a21(0), a22(1), a23(0) {}
	ToonzAffine(double a11, double a12, double a13,
				double a21, double a22, double a23)
		: a11(a11), a12(a12), a13(a13), a21(a21), a22(a22), a23(a23) {}
	ToonzAffine(const toonz_affine_t &affine) : a11(affine.a11), a12(affine.a12), a13(affine.a13), a21(affine.a21), a22(affine.a22), a23(affine.a23) {}
	ToonzAffine(const ToonzAffine &toonzAffine)
		: a11(toonzAffine.a11), a12(toonzAffine.a12), a13(toonzAffine.a13),
		  a21(toonzAffine.a21), a22(toonzAffine.a22), a23(toonzAffine.a23) {}

	static bool equals(double a, double b, double err = 1e-9)
	{
		return std::abs(a - b) < err;
	}

	ToonzAffine operator*(const ToonzAffine &toonzAffine) const;
	ToonzAffine &operator=(const ToonzAffine &toonzAffine);
	ToonzAffine &operator*=(const ToonzAffine &toonzAffine);
	bool operator==(const ToonzAffine &toonzAffine) const;
	bool operator!=(const ToonzAffine &toonzAffine) const;

	ToonzPoint operator*(const ToonzPoint &p) const;
	ToonzRect operator*(const ToonzRect &p) const;
	ToonzAffine inv() const;
	double det() const;
	bool isIdentity(double err = 1e-9) const;
	bool isTranslation(double err = 1e-9) const;
	bool isIsotropic(double err = 1e-9) const;
	ToonzAffine place(double u, double v, double x, double y) const;
};

inline ToonzPoint ToonzAffine::operator*(const ToonzPoint &pt) const
{
	return ToonzPoint(pt.x * a11 + pt.y * a12 + a13, pt.x * a21 + pt.y * a22 + a23);
}

inline ToonzRect ToonzAffine::operator*(const ToonzRect &r) const
{
	if (r.x0 == -std::numeric_limits<double>::max() ||
		r.y0 == -std::numeric_limits<double>::max() ||
		r.x1 == std::numeric_limits<double>::max() ||
		r.y1 == std::numeric_limits<double>::max())
		return ToonzRect(-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
	ToonzPoint p0 = this->operator*(ToonzPoint(r.x0, r.y0));
	ToonzPoint p1 = this->operator*(ToonzPoint(r.x1, r.y0));
	ToonzPoint p2 = this->operator*(ToonzPoint(r.x0, r.y1));
	ToonzPoint p3 = this->operator*(ToonzPoint(r.x1, r.y1));
	return ToonzRect(std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x)), std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y)),
					 std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x)), std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y)));
}

ToonzAffine ToonzAffine::operator*(const ToonzAffine &toonzAffine) const
{
	return ToonzAffine(
		a11 * toonzAffine.a11 + a12 * toonzAffine.a21,
		a11 * toonzAffine.a12 + a12 * toonzAffine.a22,
		a11 * toonzAffine.a13 + a12 * toonzAffine.a23 + a13,
		a21 * toonzAffine.a11 + a22 * toonzAffine.a21,
		a21 * toonzAffine.a12 + a22 * toonzAffine.a22,
		a21 * toonzAffine.a13 + a22 * toonzAffine.a23 + a23);
}

ToonzAffine &ToonzAffine::operator=(const ToonzAffine &toonzAffine)
{
	a11 = toonzAffine.a11;
	a12 = toonzAffine.a12;
	a13 = toonzAffine.a13;
	a21 = toonzAffine.a21;
	a22 = toonzAffine.a22;
	a23 = toonzAffine.a23;
	return *this;
}

ToonzAffine &ToonzAffine::operator*=(const ToonzAffine &toonzAffine)
{
	return *this = *this * toonzAffine;
}

bool ToonzAffine::operator==(const ToonzAffine &toonzAffine) const
{
	return equals(a11, toonzAffine.a11) && equals(a12, toonzAffine.a12) &&
		   equals(a13, toonzAffine.a13) && equals(a21, toonzAffine.a21) &&
		   equals(a22, toonzAffine.a22) && equals(a23, toonzAffine.a23);
}

bool ToonzAffine::operator!=(const ToonzAffine &toonzAffine) const
{
	return !(*this == toonzAffine);
}

ToonzAffine ToonzAffine::inv() const
{
	if (equals(a12, 0.0) && equals(a21, 0.0)) {
		assert(!equals(a11, 0.0, DBL_EPSILON));
		assert(!equals(a22, 0.0, DBL_EPSILON));
		double inv_a11 = 1.0 / a11;
		double inv_a22 = 1.0 / a22;
		return ToonzAffine(
			inv_a11, 0.0, -a13 * inv_a11,
			0.0, inv_a22, -a23 * inv_a22);
	} else if (equals(a11, 0.0) && equals(a22, 0.0)) {
		assert(!equals(a12, 0.0, DBL_EPSILON));
		assert(!equals(a21, 0.0, DBL_EPSILON));
		double inv_a21 = 1.0 / a21;
		double inv_a12 = 1.0 / a12;
		return ToonzAffine(
			0.0, inv_a21, -a23 * inv_a21,
			inv_a12, 0.0, -a13 * inv_a12);
	}
	double inv_det = 1.0 / det();
	return ToonzAffine(
		a22 * inv_det, -a12 * inv_det, (a12 * a23 - a22 * a13) * inv_det,
		-a21 * inv_det, a11 * inv_det, (a21 * a13 - a11 * a23) * inv_det);
}

double ToonzAffine::det() const
{
	return a11 * a22 - a12 * a21;
}

bool ToonzAffine::isIdentity(double err) const
{
	double value =
		(a11 - 1.0) * (a11 - 1.0) +
		(a22 - 1.0) * (a22 - 1.0) +
		a12 * a12 + a13 * a13 +
		a21 * a21 + a23 * a23;
	return value < err;
}

bool ToonzAffine::isTranslation(double err) const
{
	double value =
		(a11 - 1.0) * (a11 - 1.0) +
		(a22 - 1.0) * (a22 - 1.0) +
		a12 * a12 + a21 * a21;
	return value < err;
}

bool ToonzAffine::isIsotropic(double err) const
{
	if (equals(a11, a22, err) && equals(a12, -a21, err)) {
		return true;
	}
	return false;
}

ToonzAffine ToonzAffine::place(double u, double v, double x, double y) const
{
	return ToonzAffine(
		a11, a12, x - (a11 * u + a12 * v),
		a21, a22, y - (a21 * u + a22 * v));
}

#endif

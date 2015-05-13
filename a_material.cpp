#include "a_material.h"

RGB::RGB()
	: r(0.f)
	, g(0.f)
	, b(0.f)
{
}

RGB::RGB(float const r, float const g, float const b)
	: r(r)
	, g(g)
	, b(b)
{
}

RGB operator+(RGB const lhs, RGB const rhs)
{
	return RGB(lhs.r + rhs.r, lhs.g + rhs.g, lhs.b + rhs.b);
}

RGB operator*(RGB const lhs, RGB const rhs)
{
	return RGB(lhs.r * rhs.r, lhs.g * rhs.g, lhs.b * rhs.b);
}

RGB operator*(float const s, RGB const rgb)
{
	return RGB(s * rgb.r, s * rgb.g, s * rgb.b);
}

RGB operator*(RGB const rgb, float const s)
{
	return RGB(rgb.r * s, rgb.g * s, rgb.b * s);
}

RGB& operator+=(RGB& lhs, RGB const rhs)
{
	lhs = lhs + rhs;
	return lhs;
}

Material::Material(RGB diffuse, RGB emissive)
	: diffuse(diffuse)
	, emissive(emissive)
{
}

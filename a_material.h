#pragma once

#include "a_math.h"

struct RGB
{
	float r;
	float g;
	float b;

public:
	RGB();
	RGB(float r, float g, float b);
};

RGB operator+(RGB lhs, RGB rhs);
RGB operator*(RGB lhs, RGB rhs);
RGB operator*(float s, RGB rgb);
RGB operator*(RGB rgb, float s);
RGB operator/(RGB rgb, float s);

RGB& operator+=(RGB& lhs, RGB rhs);
RGB& operator*=(RGB& lhs, RGB rhs);
RGB& operator*=(RGB& lhs, float s);
RGB& operator/=(RGB& lhs, float s);

struct Material
{
	RGB diffuse;
	RGB emissive;

public:
	Material();
	Material(RGB diffuse, RGB emissive);
};

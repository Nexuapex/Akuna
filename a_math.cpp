#include "a_math.h"

#include <math.h>

Vec3::Vec3()
	: x(0.f)
	, y(0.f)
	, z(0.f)
{
}

Vec3::Vec3(float const x, float const y, float const z)
	: x(x)
	, y(y)
	, z(z)
{
}

Vec3 operator+(Vec3 const lhs, Vec3 const rhs)
{
	return Vec3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

Vec3 operator-(Vec3 const lhs, Vec3 const rhs)
{
	return Vec3(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

Vec3 operator*(float const s, Vec3 const v)
{
	return Vec3(s * v.x, s * v.y, s * v.z);
}

Vec3 operator*(Vec3 const v, float const s)
{
	return Vec3(v.x * s, v.y * s, v.z * s);
}

float dot(Vec3 const lhs, Vec3 const rhs)
{
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

Vec3 cross(Vec3 const lhs, Vec3 const rhs)
{
	return Vec3(
		lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.x * rhs.y - lhs.y * rhs.x
	);
}

float length_sqr(Vec3 const v)
{
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

float length(Vec3 const v)
{
	return sqrtf(length_sqr(v));
}

float length_rcp(Vec3 const v)
{
	return 1.f / length(v);
}

Vec3 normalize(Vec3 const v)
{
	return v * length_rcp(v);
}

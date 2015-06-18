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

Vec3 operator-(Vec3 const v)
{
	return Vec3(-v.x, -v.y, -v.z);
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

Mat33::Mat33()
	: col{
		Vec3(1.f, 0.f, 0.f),
		Vec3(0.f, 1.f, 0.f),
		Vec3(0.f, 0.f, 1.f),
	}
{
}

Mat33::Mat33(Vec3 const a, Vec3 const b, Vec3 const c)
	: col{a, b, c}
{
}


Vec3 transform_vector(Mat33 const& lhs, Vec3 const rhs)
{
	return Vec3(dot(lhs.col[0], rhs), dot(lhs.col[1], rhs), dot(lhs.col[2], rhs));
}

Vec3 inv_ortho_transform_vector(Mat33 const& lhs, Vec3 const rhs)
{
	Vec3 const row0(lhs.col[0].x, lhs.col[1].x, lhs.col[2].x);
	Vec3 const row1(lhs.col[0].y, lhs.col[1].y, lhs.col[2].y);
	Vec3 const row2(lhs.col[0].z, lhs.col[1].z, lhs.col[2].z);
	return Vec3(dot(row0, rhs), dot(row1, rhs), dot(row2, rhs));
}

#pragma once

struct Vec3
{
	float x;
	float y;
	float z;

public:
	Vec3();
	Vec3(float x, float y, float z);
};

Vec3 operator+(Vec3 lhs, Vec3 rhs);
Vec3 operator-(Vec3 lhs, Vec3 rhs);
Vec3 operator*(float s, Vec3 v);
Vec3 operator*(Vec3 v, float s);

float dot(Vec3 lhs, Vec3 rhs);
Vec3 cross(Vec3 lhs, Vec3 rhs);

float length_sqr(Vec3 v);
float length(Vec3 v);
float length_rcp(Vec3 v);
Vec3 normalize(Vec3 v);

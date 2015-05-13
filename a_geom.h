#pragma once

#include <inttypes.h>
#include "a_math.h"

struct Ray
{
	Vec3 origin;
	Vec3 direction;

public:
	Ray(Vec3 origin, Vec3 dir);
};

struct Barycentrics
{
	float u;
	float v;
	float w;
};

struct Intersection
{
	bool valid;
	float t;
	Vec3 point;
	Vec3 normal;
	Barycentrics bary;

public:
	Intersection();
	Intersection(Ray ray, float t, Vec3 n, Barycentrics bary);
};

Intersection intersect_ray_triangle(Ray ray, uint32_t const* indices, Vec3 const* vertices);

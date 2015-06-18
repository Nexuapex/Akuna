#pragma once

#include <stdint.h>
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

static const uint32_t kInvalidTriangle = UINT32_MAX;

struct Intersection
{
	uint32_t triangle_index;
	float t;
	Vec3 point;
	Vec3 normal;
	Vec3 tangent;
	Barycentrics bary;

public:
	Intersection();
	Intersection(Ray ray, float t, uint32_t triangle_index, Vec3 n, Vec3 dpdu, Barycentrics bary);

	bool valid() const;
};

Intersection intersect_ray_triangle(Ray ray, uint32_t triangle_index, uint32_t const* indices, Vec3 const* vertices);

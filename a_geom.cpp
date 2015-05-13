#include "a_geom.h"

#include <float.h>

Ray::Ray(Vec3 const origin, Vec3 const dir)
	: origin(origin)
	, direction(normalize(dir))
{
}

Intersection::Intersection()
	: valid(false)
	, t(FLT_MAX)
	, point(FLT_MAX, FLT_MAX, FLT_MAX)
	, normal(0.f, 0.f, 0.f)
	, bary()
{
}

Intersection::Intersection(Ray const ray, float const t, Vec3 const n, Barycentrics const bary)
	: valid(true)
	, t(t)
	, point(ray.origin + t * ray.direction)
	, normal(normalize(n))
	, bary(bary)
{
}

Intersection intersect_ray_triangle(Ray const ray, uint32_t const* indices, Vec3 const* vertices)
{
	Vec3 const a = vertices[indices[0]];
	Vec3 const b = vertices[indices[1]];
	Vec3 const c = vertices[indices[2]];

	Vec3 const q = ray.origin;
	Vec3 const p = ray.origin + ray.direction;

	Vec3 const ab = b - a;
	Vec3 const ac = c - a;
	Vec3 const qp = p - q;

	Vec3 const n = cross(ab, ac);

	float const d = dot(qp, n);
	if (d <= 0.f) return Intersection();

	Vec3 const ap = p - a;
	float t = dot(ap, n);

	Vec3 const e = cross(qp, ap);
	float v = dot(ac, e);
	if (v < 0.f || v > d) return Intersection();
	float w = -dot(ab, e);
	if (w < 0.f || v + w > d) return Intersection();

	float const ood = 1.f / d;
	t *= ood;
	v *= ood;
	w *= ood;
	float const u = 1.f - v - w;

	Barycentrics bary;
	bary.u = u;
	bary.v = v;
	bary.w = w;
	return Intersection(ray, t, n, bary);
}

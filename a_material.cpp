#include "a_material.h"

#include <math.h>

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

RGB operator/(RGB const rgb, float const s)
{
	return RGB(rgb.r / s, rgb.g / s, rgb.b / s);
}

RGB& operator+=(RGB& lhs, RGB const rhs)
{
	lhs = lhs + rhs;
	return lhs;
}

RGB& operator*=(RGB& lhs, RGB const rhs)
{
	lhs = lhs * rhs;
	return lhs;
}

RGB& operator*=(RGB& lhs, float const s)
{
	lhs = lhs * s;
	return lhs;
}

RGB& operator/=(RGB& lhs, float const s)
{
	lhs = lhs / s;
	return lhs;
}

Material::Material()
	: diffuse()
	, emissive()
	, is_light(false)
{
}

Vec3 uniform_hemisphere_sample(float const u1, float const u2)
{
	float const pi = 3.14159265358979323846f;

	float const z = u1;
	float const r = sqrtf(fmaxf(0.f, 1.f - z*z));
	float const phi = 2.f * pi * u2;
	float const x = r * cosf(phi);
	float const y = r * sinf(phi);

	return Vec3(x, y, z);
}

float uniform_hemisphere_probability_density()
{
	float const inv_2pi = 0.159154943091895335769f;
	return inv_2pi; // Probability with respect to solid angle is uniform.
}

Vec3 cosine_hemisphere_sample(float const u1, float const u2)
{
	float const pi = 3.14159265358979323846f;

	float const r = sqrtf(u1);
	float const theta = 2.f * pi * u2;
	float const x = r * cosf(theta);
	float const y = r * sinf(theta);
	float const z = sqrtf(fmaxf(0.f, 1.f - x*x - y*y));

	return Vec3(x, y, z);
}

float cosine_hemisphere_probability_density(Vec3 const normal, Vec3 const direction)
{
	float const inv_pi = 0.318309886183790671538f;
	return dot(normal, direction) * inv_pi;
}

RGB lambert_brdf_reflectance(Material const& material)
{
	float const inv_pi = 0.318309886183790671538f;
	return material.diffuse * inv_pi;
}

float lambert_brdf_probability_density(Vec3 const normal, Vec3 const direction)
{
	return cosine_hemisphere_probability_density(normal, direction);
}

BsdfSample lambert_brdf_sample(Material const& material, Vec3 const normal, Vec3 const tangent, float const u1, float const u2)
{
	Mat33 const world_from_local(tangent, cross(normal, tangent), normal);
	Vec3 const local_direction = cosine_hemisphere_sample(u1, u2);
	Vec3 const world_direction = inv_ortho_transform_vector(world_from_local, local_direction);

	BsdfSample bsdf_sample;
	bsdf_sample.direction = world_direction;
	bsdf_sample.reflectance = lambert_brdf_reflectance(material);
	bsdf_sample.probability_density = lambert_brdf_probability_density(normal, world_direction);
	return bsdf_sample;
}

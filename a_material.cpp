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

RGB operator-(RGB const lhs, RGB const rhs)
{
	return RGB(lhs.r - rhs.r, lhs.g - rhs.g, lhs.b - rhs.b);
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

RGB& operator-=(RGB& lhs, RGB const rhs)
{
	lhs = lhs - rhs;
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

float luminance(RGB const rgb)
{
	return .2126f * rgb.r + .7152f * rgb.g + .0722f * rgb.b;
}

Material::Material()
	: diffuse()
	, specular()
	, emissive()
	, ior(1.f)
	, roughness(0.f)
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

RGB lambert_brdf_reflectance(Material const& material, Vec3 const normal, Vec3 const incoming, Vec3 const outgoing)
{
	if (dot(incoming, normal) <= 0.f)
		return RGB();
	if (dot(outgoing, normal) <= 0.f)
		return RGB();

	float const inv_pi = 0.318309886183790671538f;
	return material.diffuse * inv_pi;
}

float lambert_brdf_probability_density(Vec3 const normal, Vec3 const incoming, Vec3 const outgoing)
{
	if (dot(incoming, normal) <= 0.f)
		return 0.f;
	if (dot(outgoing, normal) <= 0.f)
		return 0.f;

	return cosine_hemisphere_probability_density(normal, incoming);
}

BsdfSample lambert_brdf_sample(Vec3 const world_outgoing, Material const& material, Vec3 const normal, Vec3 const tangent, float const u1, float const u2)
{
	Mat33 const world_from_local(tangent, cross(normal, tangent), normal);
	Vec3 const local_incoming = cosine_hemisphere_sample(u1, u2);
	Vec3 const world_incoming = transform_vector(world_from_local, local_incoming);

	BsdfSample bsdf_sample;
	bsdf_sample.direction = world_incoming;
	bsdf_sample.reflectance = lambert_brdf_reflectance(material, normal, world_incoming, world_outgoing);
	bsdf_sample.probability_density = lambert_brdf_probability_density(normal, world_incoming, world_outgoing);
	return bsdf_sample;
}

float fresnel_exact(float const ior_incoming, float const ior_outgoing, float const i_dot_h)
{
	float const n = ior_outgoing / ior_incoming;
	float const c = i_dot_h;
	float const g_squared = n*n + c*c - 1.f;

	if (g_squared < 0.f) return 1.f; // total internal reflection

	float const g = sqrtf(g_squared);
	float const gpc = g + c;
	float const gmc = g - c;

	float const a = gmc / gpc;
	float const b = (c*gpc - 1.f) / (c*gmc + 1.f);
	return 0.5f * (a*a) * (1.f + b*b);
}

// TODO: [Walter07] clamps this term to zero in some cases.
float ggx_smith_geometry_term(float const cosine, float const alpha)
{
	float const alpha_squared = alpha * alpha;
	float const lerped = alpha_squared + (1.f - alpha_squared) * cosine * cosine;
	return (2.f * cosine) / (cosine + sqrtf(lerped));
}

float ggx_smith_geometry_uncorrelated(float const n_dot_i, float const n_dot_o, float const alpha)
{
	return ggx_smith_geometry_term(n_dot_i, alpha) * ggx_smith_geometry_term(n_dot_o, alpha);
}

float ggx_smith_normal_density(float const n_dot_h, float const alpha)
{
	float const inv_pi = 0.318309886183790671538f;

	// Every microfacet normal is in the same hemisphere as the surface normal.
	if (n_dot_h <= 0.f)
		return 0.f;

	float const alpha_squared = alpha * alpha;
	float const denom = n_dot_h * n_dot_h * (alpha_squared - 1.f) + 1.f;
	return inv_pi * alpha_squared / (denom * denom);
}

RGB ggx_smith_brdf_reflectance(Material const& material, Vec3 const normal, Vec3 const incoming, Vec3 const outgoing)
{
	if (dot(incoming, normal) <= 0.f)
		return RGB();
	if (dot(outgoing, normal) <= 0.f)
		return RGB();

	Vec3 const h = normalize(incoming + outgoing); // microfacet normal

	float const ior_incoming = 1.0002926f; // air
	float const ior_outgoing = material.ior;
	float const alpha = material.roughness;

	float const n_dot_i = dot(normal, incoming);
	float const n_dot_o = dot(normal, outgoing);
	float const n_dot_h = dot(normal, h);
	float const i_dot_h = dot(incoming, h);

	float const fresnel = fresnel_exact(ior_incoming, ior_outgoing, i_dot_h);
	float const geometry = ggx_smith_geometry_uncorrelated(n_dot_i, n_dot_o, alpha);
	float const density = ggx_smith_normal_density(n_dot_h, alpha);

	float const reflectance = (fresnel * geometry * density) / (4.f * n_dot_i * n_dot_o);
	return material.specular * reflectance;
}

Vec3 ggx_smith_sample_incoming_direction(float const u1, float const u2, Vec3 const outgoing, Material const& material)
{
	float const pi = 3.14159265358979323846f;

	float const alpha = material.roughness;
	float const theta = atanf((alpha * sqrtf(u1)) / sqrtf(1.f - u1));
	float const phi = 2.f * pi * u2;
	float const r = sinf(theta);
	float const x = r * cosf(phi);
	float const y = r * sinf(phi);
	float const z = cosf(theta);

	// TODO: explain this (PBRT p. 697)
	float const flip = (z * outgoing.z <= 0.f) ? -1.f : 1.f;

	Vec3 const h(x*flip, y*flip, z*flip); // microfacet normal

	Vec3 const incoming = reflect(outgoing, h);
	return incoming;
}

float ggx_smith_brdf_probability_density(Material const& material, Vec3 const normal, Vec3 const incoming, Vec3 const outgoing)
{
	if (dot(incoming, normal) <= 0.f)
		return 0.f;
	if (dot(outgoing, normal) <= 0.f)
		return 0.f;

	Vec3 const h = normalize(incoming + outgoing); // microfacet normal

	float const alpha = material.roughness;
	float const n_dot_h = dot(normal, h);
	float const o_dot_h = dot(outgoing, h);

	float const density = ggx_smith_normal_density(n_dot_h, alpha);

	return (density * n_dot_h) / (4.f * o_dot_h);
}

BsdfSample ggx_smith_brdf_sample(Vec3 const world_outgoing, Material const& material, Vec3 const normal, Vec3 const tangent, float const u1, float const u2)
{
	Mat33 const world_from_local(tangent, cross(normal, tangent), normal);
	Vec3 const local_outgoing = inv_ortho_transform_vector(world_from_local, world_outgoing);
	Vec3 const local_incoming = ggx_smith_sample_incoming_direction(u1, u2, local_outgoing, material);
	Vec3 const world_incoming = transform_vector(world_from_local, local_incoming);

	BsdfSample bsdf_sample;
	bsdf_sample.direction = world_incoming;
	bsdf_sample.reflectance = ggx_smith_brdf_reflectance(material, normal, world_incoming, world_outgoing);
	bsdf_sample.probability_density = ggx_smith_brdf_probability_density(material, normal, world_incoming, world_outgoing);
	return bsdf_sample;
}

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
RGB operator-(RGB lhs, RGB rhs);
RGB operator*(RGB lhs, RGB rhs);
RGB operator*(float s, RGB rgb);
RGB operator*(RGB rgb, float s);
RGB operator/(RGB rgb, float s);

RGB& operator+=(RGB& lhs, RGB rhs);
RGB& operator-=(RGB& lhs, RGB rhs);
RGB& operator*=(RGB& lhs, RGB rhs);
RGB& operator*=(RGB& lhs, float s);
RGB& operator/=(RGB& lhs, float s);

float luminance(RGB rgb);

struct Material
{
	RGB diffuse;
	RGB specular;
	RGB emissive;
	float ior;
	float roughness;
	bool is_light;

public:
	Material();
};

struct BsdfSample
{
	Vec3 direction;
	RGB reflectance;
	float probability_density;
};

RGB lambert_brdf_reflectance(Material const& material, Vec3 normal, Vec3 incoming, Vec3 outgoing);
float lambert_brdf_probability_density(Vec3 normal, Vec3 incoming, Vec3 outgoing);
BsdfSample lambert_brdf_sample(Vec3 outgoing, Material const& material, Vec3 normal, Vec3 tangent, float u1, float u2);

RGB ggx_smith_brdf_reflectance(Material const& material, Vec3 normal, Vec3 incoming, Vec3 outgoing);
float ggx_smith_brdf_probability_density(Material const& material, Vec3 normal, Vec3 incoming, Vec3 outgoing);
BsdfSample ggx_smith_brdf_sample(Vec3 outgoing, Material const& material, Vec3 normal, Vec3 tangent, float u1, float u2);

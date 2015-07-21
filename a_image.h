#pragma once

#include <stdint.h>
#include "a_material.h"

struct Image
{
	int width;
	int height;
	RGB* pixels;

	float const* cdf_u;
	float const* cdf_v;
};

bool read_rgbe(char const* path, Image& image);
bool write_rgbe(char const* path, Image const& image);

void precompute_cumulative_probability_density(Image& image);

struct LightSample
{
	uint32_t triangle_index; // for lights with geometry
	RGB radiance;
	Vec3 point;
	Vec3 normal;
	float probability_density;
};

RGB skydome_light_radiance(Image const& image, Vec3 direction);
float skydome_light_probability_density(Image const& image, Vec3 direction);
LightSample skydome_light_sample(Image const& image, float u1, float u2);

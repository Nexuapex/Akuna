#include "a_image.h"
#include "a_geom.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

int texel_u(Image const& image, float const u)
{
	float const x = (u - floorf(u)) * image.width;
	return static_cast<int>(x + .5f);
}

int texel_v(Image const& image, float const v)
{
	float const y = (v - floorf(v)) * image.height;
	return static_cast<int>(y + .5f);
}

RGB fetch_bilinear_wrap(Image const& image, float const u, float const v)
{
	int const width = image.width;
	int const height = image.height;
	RGB const* const pixels = image.pixels;

	float const x = (u - floorf(u)) * width;
	float const y = (v - floorf(v)) * height;

	int const x0 = static_cast<int>(x);
	int const y0 = static_cast<int>(y);
	int const x1 = (x0+1) % width;
	int const y1 = (y0+1) % height;

	RGB const m00 = pixels[y0 * width + x0];
	RGB const m01 = pixels[y0 * width + x1];
	RGB const m10 = pixels[y1 * width + x0];
	RGB const m11 = pixels[y1 * width + x1];

	float const tx = x - static_cast<float>(x0);
	float const ty = y - static_cast<float>(y0);

	RGB const m0 = m01 + (m00 - tx*m00);
	RGB const m1 = m11 + (m10 - tx*m10);
	return m1 + (m0 - ty*m0);
}

struct RGBE
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t e;
};

bool read_rgbe(char const* path, Image& image)
{
	FILE* const in = fopen(path, "rb");
	if (!in)
	{
		return false;
	}

	float gamma = 1.f;
	int width = 0;
	int height = 0;

	char line[128];

	if (!fgets(line, sizeof(line), in)) return false;
	if (line[0] != '#' || line[1] != '?') return false;

	for (;;)
	{
		if (!fgets(line, sizeof(line), in)) return false;
		if (0 == strcmp(line, "FORMAT=32-bit_rle_rgbe\n")) break; // pixel data follows
		if (1 == sscanf(line, "GAMMA=%g", &gamma)) continue;
	}

	if (!fgets(line, sizeof(line), in)) return false;
	if (0 != strcmp(line, "\n")) return false;

	if (!fgets(line, sizeof(line), in)) return false;
	if (2 != sscanf(line, "-Y %d +X %d", &height, &width)) return false;

	image.width = width;
	image.height = height;
	image.pixels = new RGB[width * height];

	for (int y = 0; y < height; ++y)
	{
		RGB* scanline = image.pixels + y * width;
		for (int x = 0; x < width; ++x)
		{
			RGBE rgbe;
			if (1 != fread(&rgbe, sizeof(rgbe), 1, in))
			{
				return false;
			}

			RGB& rgb = scanline[x];
			if (rgbe.e)
			{
				int const exponent = rgbe.e - 128;
				float const scale = (1.f/256.f) * ldexpf(1.f, exponent);
				rgb.r = powf(scale * rgbe.r, gamma);
				rgb.g = powf(scale * rgbe.g, gamma);
				rgb.b = powf(scale * rgbe.b, gamma);
			}
			else
			{
				rgb.r = rgb.g = rgb.b = 0.f;
			}
		}
	}

	fclose(in);
	return true;
}

// http://www.graphics.cornell.edu/online/formats/rgbe/
bool write_rgbe(char const* const path, Image const& image)
{
	FILE* const out = fopen(path, "wb");
	if (!out)
	{
		return false;
	}

	fprintf(out, "#?RADIANCE\n");
	fprintf(out, "GAMMA=%g\n", 1.0);
	fprintf(out, "EXPOSURE=%g\n", 1.0);
	fprintf(out, "FORMAT=32-bit_rle_rgbe\n");
	fprintf(out, "\n");

	int const width = image.width;
	int const height = image.height;

	fprintf(out, "-Y %d +X %d\n", height, width);
	for (int y = 0; y < height; ++y)
	{
		RGB const* scanline = image.pixels + y * width;
		for (int x = 0; x < width; ++x)
		{
			RGB const rgb = scanline[x];
			float const dominant = fmaxf(rgb.r, fmaxf(rgb.g, rgb.b));
			
			RGBE rgbe;
			if (dominant < 1e-32)
			{
				rgbe.r = rgbe.g = rgbe.b = rgbe.e = 0;
			}
			else
			{
				int exponent = INT_MIN;
				float const significand = frexpf(dominant, &exponent);
				float const scale = significand * 256.f / dominant;
				rgbe.r = static_cast<uint8_t>(scale * rgb.r);
				rgbe.g = static_cast<uint8_t>(scale * rgb.g);
				rgbe.b = static_cast<uint8_t>(scale * rgb.b);
				rgbe.e = static_cast<uint8_t>(exponent + 128);
			}
			fwrite(&rgbe, sizeof(rgbe), 1, out);
		}
	}

	fclose(out);
	return true;
}

void precompute_cumulative_probability_density(Image& image)
{
	int const width = image.width;
	int const height = image.height;
	float* const cdf_u = new float[width];
	float* const cdf_v = new float[width * height];

	float const pi = 3.14159265358979323846f;
	float const theta_step = pi / static_cast<float>(height);

	float sum_u = 0.f;
	for (int x = 0; x < width; ++x)
	{
		float sum_v = 0.f;
		float* const column_v = cdf_v + x * height;
		for (int y = 0; y < height; ++y)
		{
			float const lum = luminance(image.pixels[y * width + x]);
			float const theta = (y + 0.5f) * theta_step;
			sum_v += lum * sinf(theta);
			column_v[y] = sum_v;
		}
		sum_u += sum_v;
		cdf_u[x] = sum_u;
	}

	image.cdf_u = cdf_u;
	image.cdf_v = cdf_v;
}

RGB skydome_light_radiance(Image const& image, Vec3 const direction)
{
	float const inv_pi = 0.318309886183790671538f;
	float const inv_2pi = 0.159154943091895335769f;

	float const u = atan2f(direction.y, direction.x) * inv_2pi;
	float const v = acosf(direction.z) * inv_pi;

	return fetch_bilinear_wrap(image, u, v);
}

float skydome_light_probability_density(Image const& image, int const x, int const y)
{
	int const width = image.width;
	int const height = image.height;

	float const pi = 3.14159265358979323846f;
	float const theta_step = pi / static_cast<float>(height);
	float const normalization_factor = (2.f * pi * pi) / static_cast<float>(width * height);

	float const* const cdf_u = image.cdf_u;
	float const* const cdf_v = image.cdf_v + x * height;

	float const probability_density_u = ((x) ? cdf_u[x] - cdf_u[x-1] : cdf_u[0]) / cdf_u[width-1];
	float const probability_density_v = ((y) ? cdf_v[y] - cdf_v[y-1] : cdf_v[0]) / cdf_v[height-1];

	float const theta = (y + 0.5f) * theta_step;
	return (probability_density_u * probability_density_v * sinf(theta)) / normalization_factor;
}

float skydome_light_probability_density(Image const& image, Vec3 const direction)
{
	float const inv_pi = 0.318309886183790671538f;
	float const inv_2pi = 0.159154943091895335769f;
	
	float const u = atan2f(direction.y, direction.x) * inv_2pi;
	float const v = acosf(direction.z) * inv_pi;

	int const x = texel_u(image, u);
	int const y = texel_v(image, v);

	return skydome_light_probability_density(image, x, y);
}

LightSample skydome_light_sample(Image const& image, float const u1, float const u2)
{
	int const width = image.width;
	int const height = image.height;

	float const pi = 3.14159265358979323846f;
	float const phi_step = (2.f * pi) / static_cast<float>(width);
	float const theta_step = pi / static_cast<float>(height);

	float const* const cdf_u = image.cdf_u;
	float const* const pos_u = std::lower_bound(cdf_u, cdf_u + width, u1 * cdf_u[width-1]);
	int const idx_u = pos_u - cdf_u;

	float const* const cdf_v = image.cdf_v + idx_u * height;
	float const* const pos_v = std::lower_bound(cdf_v, cdf_v + height, u2 * cdf_v[height-1]);
	int const idx_v = pos_v - cdf_v;
	
	float const phi = (idx_u + 0.5f) * phi_step;
	float const theta = (idx_v + 0.5f) * theta_step;
	float const r = sinf(theta);
	float const x = r * cosf(phi);
	float const y = r * sinf(phi);
	float const z = cosf(theta);

	Vec3 const direction(x, y, z);
	float const radius = 50.f; // TODO: fiddle with this

	LightSample light_sample = {};
	light_sample.triangle_index = kInvalidTriangle;
	light_sample.radiance = image.pixels[idx_v * width + idx_u];
	light_sample.point = direction * radius;
	light_sample.normal = direction;
	light_sample.probability_density = skydome_light_probability_density(image, idx_u, idx_v);
	return light_sample;
}

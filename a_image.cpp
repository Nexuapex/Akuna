#include "a_image.h"
#include "a_material.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

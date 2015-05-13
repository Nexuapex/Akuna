#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <random>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "a_geom.h"

struct RGB
{
	float r;
	float g;
	float b;

public:
	RGB();
	RGB(float r, float g, float b);
};

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

RGB& operator+=(RGB& lhs, RGB const rhs)
{
	lhs.r += rhs.r;
	lhs.g += rhs.g;
	lhs.b += rhs.b;
	return lhs;
}

RGB operator*(RGB const rgb, float const s)
{
	return RGB(rgb.r * s, rgb.b * s, rgb.g * s);
}

struct RGBE
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t e;
};

// http://www.graphics.cornell.edu/online/formats/rgbe/
void write_rgbe(FILE* const out, int const width, int const height, RGB const image[])
{
	fprintf(out, "#?RADIANCE\n");
	fprintf(out, "GAMMA=%g\n", 1.0);
	fprintf(out, "EXPOSURE=%g\n", 1.0);
	fprintf(out, "FORMAT=32-bit_rle_rgbe\n");
	fprintf(out, "\n");

	fprintf(out, "-Y %d +X %d\n", height, width);
	for (int y = 0; y < height; ++y)
	{
		RGB const* scanline = image + y * width;
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
}

struct Scene
{
	uint32_t triangle_count;
	uint32_t* indices;
	Vec3 const* vertices;
};

Intersection intersect_scene(Ray const ray, Scene const& scene)
{
	Intersection intersect;
	for (uint32_t triangle_index = 0; triangle_index < scene.triangle_count; ++triangle_index)
	{
		Intersection const tri_intersect = intersect_ray_triangle(ray, triangle_index, scene.indices, scene.vertices);
		if (tri_intersect.t < intersect.t)
		{
			intersect = tri_intersect;
		}
	}
	return intersect;
}

struct CameraSample
{
	float x;
	float y;
};

CameraSample random_camera_sample(int const x, int const y, int const width, int const height, std::mt19937& random_engine)
{
	std::uniform_real_distribution<float> distrib(0.f, 1.f); // [0, 1)

	CameraSample camera_sample;
	camera_sample.x = (static_cast<float>(x) + distrib(random_engine)) / static_cast<float>(width) * 2.f - 1.f;
	camera_sample.y = (static_cast<float>(y) + distrib(random_engine)) / static_cast<float>(height) * 2.f - 1.f;
	return camera_sample;
}

RGB sample_image(Vec3 const camera_position, CameraSample const camera_sample, Scene const& scene)
{
	Vec3 const image_plane_position(camera_sample.x, camera_sample.y, 1.f);
	Ray const initial_ray(camera_position, image_plane_position);

	Intersection const intersect = intersect_scene(initial_ray, scene);
	if (intersect.valid())
	{
		return RGB(1.f, 0.f, 0.f);
	}
	else
	{
		return RGB();
	}
}

int main(int const argc, char const* const argv[])
{
	(void)argc;
	(void)argv;

	Scene scene = {};

	Assimp::Importer importer;
	if (aiScene const* const imp_scene = importer.ReadFile("scene.nff", aiProcess_Triangulate | aiProcess_SortByPType))
	{
		int const meshCount = imp_scene->mNumMeshes;
		for (int i = 0; i < meshCount; ++i)
		{
			aiMesh const* const imp_mesh = imp_scene->mMeshes[i];
			if (aiPrimitiveType_TRIANGLE == imp_mesh->mPrimitiveTypes)
			{
				scene.triangle_count = imp_mesh->mNumFaces;
				scene.indices = new uint32_t[scene.triangle_count * 3];
				for (uint32_t triangle_index = 0; triangle_index < scene.triangle_count; ++triangle_index)
					memcpy(scene.indices + triangle_index * 3, imp_mesh->mFaces[triangle_index].mIndices, sizeof(uint32_t) * 3);
				scene.vertices = reinterpret_cast<Vec3 const*>(imp_mesh->mVertices);
			}
		}
	}
	else
	{
		fprintf(stderr, "%s\n", importer.GetErrorString());
	}

	std::mt19937 random_engine;
	Vec3 const camera_position(0.f, 0.f, 0.f);

	int const width = 256;
	int const height = 256;
	int const samples_per_pixel = 16;
	float const sample_weight = 1.f / static_cast<float>(samples_per_pixel);

	RGB image[width][height] = {};

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			for (int n = 0; n < samples_per_pixel; ++n)
			{
				CameraSample const camera_sample = random_camera_sample(x, y, width, height, random_engine);
				RGB const sample = sample_image(camera_position, camera_sample, scene);
				image[x][y] += sample * sample_weight;
			}
		}
	}

	if (FILE* const out = fopen("test.hdr", "wb"))
	{
		write_rgbe(out, width, height, &image[0][0]);
		fclose(out);
	}
	return 0;
}

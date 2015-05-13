#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <random>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "a_geom.h"
#include "a_material.h"

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
	uint32_t const* indices;
	Vec3 const* vertices;
	Material const* materials;
	uint8_t const* material_indices;
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

struct DistributionSample
{
	Vec3 direction;
	float contribution;

public:
	DistributionSample();
};

DistributionSample::DistributionSample()
	: direction(0.f, 0.f, 0.f)
	, contribution(0.f)
{
}

DistributionSample random_hemisphere_sample(Vec3 const normal, std::mt19937& random_engine)
{
	std::uniform_real_distribution<float> distrib(-1.f, 1.f); // [-1, 1)

	Vec3 const dir = normalize(Vec3(distrib(random_engine), distrib(random_engine), distrib(random_engine)));
	float const cos_theta = dot(dir, normal);
	float const inv_pi = 0.318309886183790671538f;

	DistributionSample distribution_sample;
	distribution_sample.direction = dir;
	distribution_sample.contribution = (cos_theta > 0.f) ? cos_theta * inv_pi : 0.f;
	return distribution_sample;
}

RGB sample_image(Vec3 const camera_position, CameraSample const camera_sample, Scene const& scene, std::mt19937& random_engine)
{
	RGB color;

	Vec3 const image_plane_position(camera_sample.x, camera_sample.y, 1.f);

	int path_length = 0;
	Ray ray(camera_position, image_plane_position);
	RGB contribution(1.f, 1.f, 1.f);

	do
	{
		++path_length;

		Intersection const intersect = intersect_scene(ray, scene);
		if (!intersect.valid())
			break; // Terminate the path.

		// Implicit path.
		//

		Material const& material = scene.materials[scene.material_indices[intersect.triangle_index]];
		RGB const emissive = material.emissive * RGB(intersect.bary.u, intersect.bary.v, intersect.bary.w);
		color += contribution * emissive;

		// Extend the path.
		//

		DistributionSample const distribution_sample = random_hemisphere_sample(intersect.normal, random_engine);
		ray = Ray(intersect.point, distribution_sample.direction);
		contribution *= material.diffuse * distribution_sample.contribution;
	} while (path_length <= 1);

	return color;
}

struct Image
{
	static int const kWidth = 256;
	static int const kHeight = 256;

	RGB pixel[kWidth][kHeight];
};

int main(int const argc, char const* const argv[])
{
	(void)argc;
	(void)argv;

	Scene scene = {};

	Material const materials[] =
	{
		Material(RGB(0.f, 0.f, 0.f), RGB(1.f, 1.f, 1.f)),
		Material(RGB(1.f, 1.f, 1.f), RGB(0.f, 0.f, 0.f)),
	};

	Assimp::Importer importer;
	if (aiScene const* const imp_scene = importer.ReadFile("scene.nff", aiProcess_Triangulate | aiProcess_SortByPType))
	{
		int const meshCount = imp_scene->mNumMeshes;
		for (int i = 0; i < meshCount; ++i)
		{
			aiMesh const* const imp_mesh = imp_scene->mMeshes[i];
			if (aiPrimitiveType_TRIANGLE != imp_mesh->mPrimitiveTypes)
				continue;

			uint32_t const triangle_count = imp_mesh->mNumFaces;
			uint32_t const index_count = 3 * triangle_count;
			uint32_t* const indices = new uint32_t[index_count];
			Vec3 const* const vertices = reinterpret_cast<Vec3 const*>(imp_mesh->mVertices);
			uint8_t* const material_indices = new uint8_t[triangle_count];

			for (uint32_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index)
			{
				memcpy(indices + 3 * triangle_index, imp_mesh->mFaces[triangle_index].mIndices, 3 * sizeof(uint32_t));
				material_indices[triangle_index] = (0 == triangle_index) ? 0 : 1; // First triangle has a special material.
			}

			scene.triangle_count = triangle_count;
			scene.indices = indices;
			scene.vertices = vertices;
			scene.materials = materials;
			scene.material_indices = material_indices;
		}
	}
	else
	{
		fprintf(stderr, "%s\n", importer.GetErrorString());
	}

	std::mt19937 random_engine;
	Vec3 const camera_position(0.f, 0.f, 0.f);

	int const samples_per_pixel = 16;
	float const sample_weight = 1.f / static_cast<float>(samples_per_pixel);

	Image* const image = new Image;

	for (int y = 0; y < Image::kHeight; ++y)
	{
		for (int x = 0; x < Image::kWidth; ++x)
		{
			for (int n = 0; n < samples_per_pixel; ++n)
			{
				CameraSample const camera_sample = random_camera_sample(x, y, Image::kWidth, Image::kHeight, random_engine);
				RGB const sample = sample_image(camera_position, camera_sample, scene, random_engine);
				image->pixel[x][y] += sample * sample_weight;
			}
		}
	}

	if (FILE* const out = fopen("test.hdr", "wb"))
	{
		write_rgbe(out, Image::kWidth, Image::kHeight, &image->pixel[0][0]);
		fclose(out);
	}
	return 0;
}

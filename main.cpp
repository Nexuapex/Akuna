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

struct Light
{
	uint32_t triangle_index;
	uint32_t triangle_count;
};

struct Scene
{
	uint32_t triangle_count;
	uint32_t light_count;

	uint32_t const* indices;
	Vec3 const* vertices;
	Material const* materials;
	uint8_t const* material_indices;

	Light const* lights;
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

struct TriangleSample
{
	Vec3 point;
	Vec3 normal;
	float area;
};

TriangleSample random_triangle_sample(uint32_t const triangle_index, Scene const& scene, std::mt19937& random_engine)
{
	std::uniform_real_distribution<float> distrib(0.f, 1.f); // [0, 1)

	float const u1 = distrib(random_engine);
	float const u2 = distrib(random_engine);
	float const su1 = sqrtf(u1);

	Barycentrics bary;
	bary.u = 1.f - su1;
	bary.v = u2 * su1;
	bary.w = 1.f - bary.u - bary.v;

	uint32_t const base_index = 3u * triangle_index;
	uint32_t const* indices = scene.indices;
	Vec3 const* vertices = scene.vertices;

	Vec3 const a = vertices[indices[base_index + 0]];
	Vec3 const b = vertices[indices[base_index + 1]];
	Vec3 const c = vertices[indices[base_index + 2]];

	Vec3 const ab = b - a;
	Vec3 const ac = c - a;

	Vec3 const n = cross(ab, ac);

	TriangleSample triangle_sample = {};
	triangle_sample.point = bary.u * a + bary.v * b + bary.w * c;
	triangle_sample.normal = normalize(n);
	triangle_sample.area = 0.5f * length(n);
	return triangle_sample;
}

struct LightSample
{
	uint32_t triangle_index;
	Vec3 point;
	Vec3 normal;
	float probability_density;
	Material const* material;
};

LightSample sample_scene_light(Scene const& scene, std::mt19937& random_engine)
{
	std::uniform_int_distribution<uint32_t> light_distrib(0u, scene.light_count - 1); // TODO: sample by area.
	uint32_t const light_index = light_distrib(random_engine);
	Light const& light = scene.lights[light_index];

	std::uniform_int_distribution<uint32_t> triangle_distrib(0u, light.triangle_count - 1); // TODO: sample by area.
	uint32_t const triangle_index = light.triangle_index + triangle_distrib(random_engine);
	uint8_t const material_index = scene.material_indices[triangle_index];

	TriangleSample const triangle_sample = random_triangle_sample(triangle_index, scene, random_engine);

	LightSample light_sample = {};
	light_sample.triangle_index = triangle_index;
	light_sample.point = triangle_sample.point;
	light_sample.normal = triangle_sample.normal;
	light_sample.probability_density = 1.f / (scene.light_count * light.triangle_count * triangle_sample.area); // TODO: sample by area.
	light_sample.material = &scene.materials[material_index];
	return light_sample;
}

struct CameraSample
{
	float x;
	float y;
};

CameraSample random_camera_sample(int const x, int const y, int const width, int const height, std::mt19937& random_engine)
{
	std::uniform_real_distribution<float> distrib(0.f, 1.f); // [0, 1)

	CameraSample camera_sample = {};
	camera_sample.x = (static_cast<float>(x) + distrib(random_engine)) / static_cast<float>(width)  * 2.f - 1.f;
	camera_sample.y = (static_cast<float>(y) + distrib(random_engine)) / static_cast<float>(height) * 2.f - 1.f;
	camera_sample.y *= -1.f;
	return camera_sample;
}

BsdfSample surface_bsdf_sample(Vec3 const outgoing, Material const& material, Vec3 const normal, Vec3 const tangent, std::mt19937& random_engine)
{
	std::uniform_real_distribution<float> sample_distrib(0.f, 1.f); // [0, 1)

	float const u1 = sample_distrib(random_engine);
	float const u2 = sample_distrib(random_engine);

	std::uniform_int_distribution<> bsdf_distrib(0, 1);

	BsdfSample const lambert_sample = lambert_brdf_sample(material, normal, tangent, u1, u2);
	BsdfSample const ggx_smith_sample = ggx_smith_brdf_sample(outgoing, material, normal, tangent, u1, u2);

	BsdfSample bsdf_sample;

	switch (bsdf_distrib(random_engine))
	{
	case 0:
		bsdf_sample.direction = lambert_sample.direction;
		bsdf_sample.reflectance = lambert_sample.reflectance + ggx_smith_brdf_reflectance(material, normal, lambert_sample.direction, outgoing);
		bsdf_sample.probability_density = 0.5f * (lambert_sample.probability_density + ggx_smith_brdf_probability_density(normal, lambert_sample.direction));
		break;
	case 1:
		bsdf_sample.direction = ggx_smith_sample.direction;
		bsdf_sample.reflectance = lambert_brdf_reflectance(material) + ggx_smith_sample.reflectance;
		bsdf_sample.probability_density = 0.5f * (lambert_brdf_probability_density(normal, ggx_smith_sample.direction) + ggx_smith_sample.probability_density);
		break;
	}

	return bsdf_sample;
}

RGB surface_bsdf_reflectance(Material const& material, Vec3 const normal, Vec3 const incoming, Vec3 const outgoing)
{
	return lambert_brdf_reflectance(material) + ggx_smith_brdf_reflectance(material, normal, incoming, outgoing);
}

bool sample_russian_roulette(float const continue_probability, std::mt19937& random_engine)
{
	std::uniform_real_distribution<float> distrib(0.f, 1.f); // [0, 1)
	return distrib(random_engine) > continue_probability;
}

RGB sample_image(Vec3 const camera_position, CameraSample const camera_sample, Scene const& scene, std::mt19937& random_engine)
{
	RGB color;

	Vec3 const image_plane_direction(camera_sample.x, camera_sample.y, -1.f);
	float const continue_probability = 0.8f;

	int path_length = 0;
	Ray ray(camera_position, image_plane_direction);
	RGB path_throughput(1.f, 1.f, 1.f);

	for (;;)
	{
		++path_length;

		Intersection const intersect = intersect_scene(ray, scene);
		if (!intersect.valid())
			break; // Terminate the path.

		Material const& material = scene.materials[scene.material_indices[intersect.triangle_index]];
		Vec3 const biased_point = intersect.point + intersect.normal * 1e-3f; // Avoid acne from self-shadowing.

		// Implicit path.
		//

		{
			RGB const implicit_path_sample = path_throughput * material.emissive;
			color += ((path_length > 1) ? .5f : 1.f) * implicit_path_sample;
		}

		// Explicit path.
		//

		{
			LightSample const light_sample = sample_scene_light(scene, random_engine); // TODO: importance sampling.
			Ray const light_ray(biased_point, light_sample.point - biased_point);
			Material const& light_material = *light_sample.material;
			float const cosine_factor = dot(light_ray.direction, intersect.normal);
			if (cosine_factor > 0.f)
			{
				Intersection const light_intersect = intersect_scene(light_ray, scene); // TODO: this should be a line test.
				if (!light_intersect.valid() || light_intersect.triangle_index == light_sample.triangle_index)
				{
					float const light_cosine_factor = dot(-light_ray.direction, light_sample.normal);
					if (light_cosine_factor > 0.f)
					{
						RGB const reflectance = surface_bsdf_reflectance(material, intersect.normal, light_ray.direction, -ray.direction);
						RGB const extended_path_throughput = path_throughput * reflectance * cosine_factor;
						float const geometric_factor = light_cosine_factor / length_sqr(light_sample.point - biased_point);
						RGB const explicit_path_sample = extended_path_throughput * light_material.emissive * (geometric_factor / light_sample.probability_density);
						color += .5f * explicit_path_sample;
					}
				}
			}
		}

		// Possibly terminate the path.
		//

		if (path_length > 1)
		{
			if (sample_russian_roulette(continue_probability, random_engine))
				break;
			path_throughput /= continue_probability;
		}

		// Extend the path.
		//

		BsdfSample const bsdf_sample = surface_bsdf_sample(-ray.direction, material, intersect.normal, intersect.tangent, random_engine);
		ray = Ray(biased_point, bsdf_sample.direction);
		path_throughput *= bsdf_sample.reflectance * (dot(bsdf_sample.direction, intersect.normal) / bsdf_sample.probability_density);
	}

	return color;
}

struct SceneSizes
{
	uint32_t triangle_count;
	uint32_t vertex_count;
	uint32_t light_count;
};

SceneSizes get_scene_sizes(aiScene const* const scene)
{
	uint32_t const mesh_count = scene->mNumMeshes;

	SceneSizes sizes = {};
	for (uint32_t mesh_index = 0; mesh_index < mesh_count; ++mesh_index)
	{
		aiMesh const* const mesh = scene->mMeshes[mesh_index];
		aiMaterial const* const material = scene->mMaterials[mesh->mMaterialIndex];
		if (aiPrimitiveType_TRIANGLE == mesh->mPrimitiveTypes)
		{
			sizes.triangle_count += mesh->mNumFaces;
			sizes.vertex_count += mesh->mNumVertices;

			aiColor3D emissive;
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive))
				if (!emissive.IsBlack())
					sizes.light_count++;
		}
	}
	return sizes;
}

struct Image
{
	static int const kWidth = 256;
	static int const kHeight = 256;

	RGB pixel[kHeight][kWidth];
};

int main(int const argc, char const* const argv[])
{
	(void)argc;
	(void)argv;

	Scene scene = {};

	Assimp::Importer importer;
	if (aiScene const* const imp_scene = importer.ReadFile("CornellBox-Original.obj", aiProcess_Triangulate | aiProcess_SortByPType))
	{
		SceneSizes const sizes = get_scene_sizes(imp_scene);

		uint32_t const mesh_count = imp_scene->mNumMeshes;
		uint32_t const triangle_count = sizes.triangle_count;
		uint32_t const index_count = 3 * triangle_count;
		uint32_t const vertex_count = sizes.vertex_count;
		uint32_t const material_count = imp_scene->mNumMaterials;
		uint32_t const light_count = sizes.light_count;

		uint32_t* const indices = new uint32_t[index_count];
		Vec3* const vertices = new Vec3[vertex_count];
		Material* const materials = new Material[material_count];
		uint8_t* const material_indices = new uint8_t[triangle_count];
		Light* const lights = new Light[light_count];

		for (uint32_t material_index = 0; material_index < material_count; ++material_index)
		{
			aiMaterial const* const imp_material = imp_scene->mMaterials[material_index];
			Material& material = materials[material_index];
			{
				aiColor3D diffuse;
				if (AI_SUCCESS == imp_material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse))
				{
					memcpy(&material.diffuse, &diffuse, sizeof(RGB));
				}
			}
			{
				aiColor3D specular;
				if (AI_SUCCESS == imp_material->Get(AI_MATKEY_COLOR_SPECULAR, specular))
				{
					memcpy(&material.specular, &specular, sizeof(RGB));
				}
			}
			{
				aiColor3D emissive;
				if (AI_SUCCESS == imp_material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive))
				{
					memcpy(&material.emissive, &emissive, sizeof(RGB));
					material.is_light = !emissive.IsBlack();
				}
			}
			{
				imp_material->Get(AI_MATKEY_REFRACTI, material.ior);
			}
			{
				float shininess = 0.f;
				if (AI_SUCCESS == imp_material->Get(AI_MATKEY_SHININESS, shininess))
				{
					// Fairly arbitrary remapping.
					material.roughness = sqrtf(2.f / (shininess + 2.f));
				}
			}
		}

		uint32_t base_index = 0;
		uint32_t current_triangle = 0;
		uint32_t* current_index = indices;
		Vec3* current_vertex = vertices;
		uint8_t* current_material_index = material_indices;
		Light* current_light = lights;

		for (uint32_t mesh_index = 0; mesh_index < mesh_count; ++mesh_index)
		{
			aiMesh const* const imp_mesh = imp_scene->mMeshes[mesh_index];
			if (aiPrimitiveType_TRIANGLE != imp_mesh->mPrimitiveTypes)
				continue;

			uint32_t const material_index = imp_mesh->mMaterialIndex;
			Material const& material = materials[material_index];
			if (material.is_light)
			{
				Light& light = *current_light++;
				light.triangle_index = current_triangle;
				light.triangle_count = imp_mesh->mNumFaces;
			}

			for (uint32_t triangle_index = 0; triangle_index < imp_mesh->mNumFaces; ++triangle_index)
			{
				aiFace const& imp_face = imp_mesh->mFaces[triangle_index];
				for (uint32_t index_index = 0; index_index < imp_face.mNumIndices; ++index_index)
					*current_index++ = base_index + imp_face.mIndices[index_index];
				*current_material_index++ = static_cast<uint8_t>(material_index);
			}

			memcpy(current_vertex, imp_mesh->mVertices, imp_mesh->mNumVertices * sizeof(Vec3));
			current_vertex += imp_mesh->mNumVertices;
			base_index += imp_mesh->mNumVertices;
			current_triangle += imp_mesh->mNumFaces;
		}

		scene.triangle_count = triangle_count;
		scene.light_count = light_count;

		scene.indices = indices;
		scene.vertices = vertices;
		scene.materials = materials;
		scene.material_indices = material_indices;

		scene.lights = lights;
	}
	else
	{
		fprintf(stderr, "%s\n", importer.GetErrorString());
		return 1;
	}

	std::mt19937 random_engine;
	Vec3 const camera_position(0.f, 1.f, 2.f);

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
				image->pixel[y][x] += sample * sample_weight;
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

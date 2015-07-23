#include <math.h>

#include <random>
#include <thread>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include "a_geom.h"
#include "a_image.h"
#include "a_material.h"

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
	float light_area;
	Image const* skydome;
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
	return triangle_sample;
}

SurfaceRadiance scene_light_radiance(Scene const& scene, Vec3 const direction)
{
	if (scene.skydome)
	{
		return skydome_light_radiance(*scene.skydome, direction);
	}

	SurfaceRadiance surface = {};
	surface.is_light = false;
	return surface;
}

float scene_light_probability_density(Scene const& scene, Vec3 const direction)
{
	if (scene.skydome)
	{
		return skydome_light_probability_density(*scene.skydome, direction);
	}

	return 1.f / scene.light_area;
}

LightSample scene_light_sample(Scene const& scene, std::mt19937& random_engine)
{
	if (scene.skydome)
	{
		std::uniform_real_distribution<float> distrib(0.f, 1.f); // [0, 1)

		float const u1 = distrib(random_engine);
		float const u2 = distrib(random_engine);

		return skydome_light_sample(*scene.skydome, u1, u2);
	}

	std::uniform_int_distribution<uint32_t> light_distrib(0u, scene.light_count - 1); // TODO: sample by area.
	uint32_t const light_index = light_distrib(random_engine);
	Light const& light = scene.lights[light_index];

	std::uniform_int_distribution<uint32_t> triangle_distrib(0u, light.triangle_count - 1); // TODO: sample by area.
	uint32_t const triangle_index = light.triangle_index + triangle_distrib(random_engine);
	uint8_t const material_index = scene.material_indices[triangle_index];

	TriangleSample const triangle_sample = random_triangle_sample(triangle_index, scene, random_engine);

	LightSample light_sample = {};
	light_sample.triangle_index = triangle_index;
	light_sample.radiance = scene.materials[material_index].emissive;
	light_sample.point = triangle_sample.point;
	light_sample.normal = triangle_sample.normal;
	light_sample.probability_density = scene_light_probability_density(scene, Vec3());
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

float power_heuristic(float const probability_density_f, float const probability_density_g)
{
	float const f = probability_density_f;
	float const g = probability_density_g;

	return (f*f) / (f*f + g*g);
}

BsdfSample surface_bsdf_sample(Vec3 const outgoing, Material const& material, Vec3 const normal, Vec3 const tangent, std::mt19937& random_engine)
{
	std::uniform_real_distribution<float> sample_distrib(0.f, 1.f); // [0, 1)

	float const u1 = sample_distrib(random_engine);
	float const u2 = sample_distrib(random_engine);

	std::uniform_int_distribution<> bsdf_distrib(0, 1);

	BsdfSample const lambert_sample = lambert_brdf_sample(outgoing, material, normal, tangent, u1, u2);
	BsdfSample const ggx_smith_sample = ggx_smith_brdf_sample(outgoing, material, normal, tangent, u1, u2);

	BsdfSample bsdf_sample;

	switch (bsdf_distrib(random_engine))
	{
	case 0:
		bsdf_sample.direction = lambert_sample.direction;
		bsdf_sample.reflectance = lambert_sample.reflectance + ggx_smith_brdf_reflectance(material, normal, lambert_sample.direction, outgoing);
		bsdf_sample.probability_density = 0.5f * (lambert_sample.probability_density + ggx_smith_brdf_probability_density(material, normal, lambert_sample.direction, outgoing));
		break;
	case 1:
		bsdf_sample.direction = ggx_smith_sample.direction;
		bsdf_sample.reflectance = lambert_brdf_reflectance(material, normal, ggx_smith_sample.direction, outgoing) + ggx_smith_sample.reflectance;
		bsdf_sample.probability_density = 0.5f * (lambert_brdf_probability_density(normal, ggx_smith_sample.direction, outgoing) + ggx_smith_sample.probability_density);
		break;
	}

	return bsdf_sample;
}

RGB surface_bsdf_reflectance(Material const& material, Vec3 const normal, Vec3 const incoming, Vec3 const outgoing)
{
	return lambert_brdf_reflectance(material, normal, incoming, outgoing) + ggx_smith_brdf_reflectance(material, normal, incoming, outgoing);
}

float surface_brdf_probability_density(Material const& material, Vec3 const normal, Vec3 const incoming, Vec3 const outgoing)
{
	return 0.5f * (lambert_brdf_probability_density(normal, incoming, outgoing) + ggx_smith_brdf_probability_density(material, normal, incoming, outgoing));
}

bool sample_russian_roulette(float const continue_probability, std::mt19937& random_engine)
{
	std::uniform_real_distribution<float> distrib(0.f, 1.f); // [0, 1)
	return distrib(random_engine) > continue_probability;
}

RGB sample_image(Vec3 const camera_position, Vec3 const camera_direction, Scene const& scene, std::mt19937& random_engine)
{
	RGB color;

	float const continue_probability = 0.8f;

	int path_length = 0;
	Ray ray(camera_position, camera_direction);
	RGB path_throughput(1.f, 1.f, 1.f);
	float last_forward_sampling_probability_density = 0.f;

	for (;;)
	{
		++path_length;

		Intersection const intersect = intersect_scene(ray, scene);

		// Implicit path.
		//

		{
			SurfaceRadiance surface = {};

			if (intersect.valid())
			{
			#if 0
				Material const& material = scene.materials[scene.material_indices[intersect.triangle_index]];
				surface.is_light = material.is_light;
				surface.radiance = material.emissive;
				surface.point = intersect.point;
				surface.normal = intersect.normal;
			#else
				surface.is_light = false;
			#endif
			}
			else
			{
				surface = scene_light_radiance(scene, ray.direction);
			}

			if (surface.is_light)
			{
				RGB const implicit_path_sample = path_throughput * surface.radiance;
				float implicit_path_weight = 1.f;
				if (path_length > 1)
				{
					float const geometric_factor = dot(-ray.direction, surface.normal) / length_sqr(surface.point - ray.origin);
					float const implicit_path_probability_density = last_forward_sampling_probability_density * geometric_factor;
					float const explicit_path_probability_density = scene_light_probability_density(scene, ray.direction);
					implicit_path_weight = power_heuristic(implicit_path_probability_density, explicit_path_probability_density);
				}
				color += implicit_path_weight * implicit_path_sample;
			}
		}

		if (!intersect.valid())
			break; // Terminate the path.

		Material const& material = scene.materials[scene.material_indices[intersect.triangle_index]];
		Vec3 const biased_point = intersect.point + intersect.normal * 1e-3f; // Avoid acne from self-shadowing.

		// Explicit path.
		//

		{
			LightSample const light_sample = scene_light_sample(scene, random_engine); // TODO: importance sampling.
			Ray const light_ray(biased_point, light_sample.point - biased_point);
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
						float const forward_sampling_probability_density = surface_brdf_probability_density(material, intersect.normal, light_ray.direction, -ray.direction);

						RGB const extended_path_throughput = path_throughput * reflectance * cosine_factor;
						float const geometric_factor = light_cosine_factor / length_sqr(light_sample.point - biased_point);
						RGB const explicit_path_sample = extended_path_throughput * light_sample.radiance * (geometric_factor / light_sample.probability_density);
						float const implicit_path_probability_density = forward_sampling_probability_density * geometric_factor;

						float const explicit_path_weight = power_heuristic(light_sample.probability_density, implicit_path_probability_density);
						color += explicit_path_weight * explicit_path_sample;
					}
				}
			}
		}

		// Possibly terminate the path.
		//

		if (path_length > 3)
		{
			if (sample_russian_roulette(continue_probability, random_engine))
				break;
			path_throughput /= continue_probability;
		}

		// Extend the path.
		//

		BsdfSample const bsdf_sample = surface_bsdf_sample(-ray.direction, material, intersect.normal, intersect.tangent, random_engine);
		if (bsdf_sample.probability_density == 0.f)
			break;
		ray = Ray(biased_point, bsdf_sample.direction);
		path_throughput *= bsdf_sample.reflectance * (dot(bsdf_sample.direction, intersect.normal) / bsdf_sample.probability_density);
		last_forward_sampling_probability_density = bsdf_sample.probability_density;
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

float get_scene_light_area(Scene const& scene)
{
	uint32_t const* indices = scene.indices;
	Vec3 const* vertices = scene.vertices;

	float light_area = 0.f;

	for (uint32_t light_index = 0; light_index < scene.light_count; ++light_index)
	{
		Light const& light = scene.lights[light_index];
		for (uint32_t triangle_index = 0; triangle_index < light.triangle_count; ++triangle_index)
		{
			uint32_t const base_index = 3u * (light.triangle_index + triangle_index);

			Vec3 const a = vertices[indices[base_index + 0]];
			Vec3 const b = vertices[indices[base_index + 1]];
			Vec3 const c = vertices[indices[base_index + 2]];

			Vec3 const ab = b - a;
			Vec3 const ac = c - a;

			Vec3 const n = cross(ab, ac);
			light_area += 0.5f * length(n);
		}
	}

	return light_area;
}

void path_trace(Scene const& scene, Image& image)
{
	std::mt19937 random_engine;
	Vec3 const camera_position(0.f, 1.f, 4.9f);
	float const image_plane_size = 0.25f;

	int const width = 256;
	int const height = 256;
	int const samples_per_pixel = 16;
	float const sample_weight = 1.f / static_cast<float>(samples_per_pixel);

	image.width = width;
	image.height = height;
	image.pixels = new RGB[image.width * image.height];

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			for (int n = 0; n < samples_per_pixel; ++n)
			{
				CameraSample const camera_sample = random_camera_sample(x, y, width, height, random_engine);
				Vec3 const image_plane_direction(camera_sample.x * image_plane_size, camera_sample.y * image_plane_size, -1.f);
				RGB const sample = sample_image(camera_position, image_plane_direction, scene, random_engine);
				image.pixels[y * width + x] += sample * sample_weight;
			}
		}
	}
}

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
		scene.light_area = get_scene_light_area(scene);
	}
	else
	{
		fprintf(stderr, "%s\n", importer.GetErrorString());
		return 1;
	}

	Image skydome = {};
	if (!read_rgbe("Barcelona_Rooftops/Barce_Rooftop_C_3k.hdr", skydome))
	{
		fputs("Failed to read skydome image\n", stderr);
		return 1;
	}
	precompute_cumulative_probability_density(skydome);
	scene.skydome = &skydome;

	unsigned int const kMaxThreadCount = 16;
	unsigned int const thread_count = std::max(std::min(std::thread::hardware_concurrency(), kMaxThreadCount) - 1u, 1u);
	Image images[kMaxThreadCount] = {};

	std::vector<std::thread> threads;
	threads.reserve(thread_count);
	for (unsigned int thread_index = 0; thread_index < thread_count; ++thread_index)
	{
		Image& image = images[thread_index];
		threads.emplace_back([&scene, &image]() { path_trace(scene, image); });
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}

	Image& final_image = images[0];
	int const pixel_count = final_image.width * final_image.height;
	float const image_weight = 1.f / static_cast<float>(thread_count);

	for (unsigned int thread_index = 1; thread_index < thread_count; ++thread_index)
	{
		Image const& image = images[thread_index];
		for (int i = 0; i < pixel_count; ++i)
		{
			final_image.pixels[i] += image.pixels[i];
		}
	}
	for (int i = 0; i < pixel_count; ++i)
	{
		final_image.pixels[i] *= image_weight;
	}

	if (!write_rgbe("test.hdr", final_image))
	{
		fputs("Failed to write image\n", stderr);
		return 1;
	}

	return 0;
}

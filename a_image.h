#pragma once

struct RGB;

struct Image
{
	int width;
	int height;
	RGB* pixels;
};

bool write_rgbe(char const* path, Image& image);

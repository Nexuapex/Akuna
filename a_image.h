#pragma once

struct RGB;

struct Image
{
	int width;
	int height;
	RGB* pixels;
};

bool read_rgbe(char const* path, Image& image);
bool write_rgbe(char const* path, Image const& image);

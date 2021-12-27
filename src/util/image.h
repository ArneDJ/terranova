#pragma once
#include "../geometry/geometry.h"

namespace util {

enum : uint8_t {
	CHANNEL_RED = 0,
	CHANNEL_GREEN = 1,
	CHANNEL_BLUE = 2,
	CHANNEL_ALPHA = 3
};

enum : uint8_t {
	COLORSPACE_GRAYSCALE = 1,
	COLORSPACE_RGB = 3,
	COLORSPACE_RGBA = 4
};

template <class T>
class Image {
public:
	void resize(uint16_t width, uint16_t height, uint8_t channels)
	{
		m_width = width;
		m_height = height;
		m_channels = channels;

		m_raster.resize(width * height * channels);

		wipe();
	}
	void wipe()
	{
		std::fill(m_raster.begin(), m_raster.end(), 0);
	}
	void copy(const Image<T> &original)
	{
		const auto &raster = original.raster();

		if (m_width != original.width() || m_height != original.height() || m_channels != original.channels() || m_raster.size() != raster.size()) {
			m_width = original.width();
			m_height = original.height();
			m_channels = original.channels();

			m_raster.resize(raster.size());
		}

		std::copy(raster.begin(), raster.end(), m_raster.begin());
	}
	void plot(uint16_t x, uint16_t y, uint8_t channel, T color)
	{
		if (channel >= m_channels) { return; }

		if (x >= m_width || y >= m_height) { return; }

		const auto index = y * m_width * m_channels + x * m_channels + channel;

		if (index >= m_raster.size()) { return; }

		m_raster[index] = color;
	}
	T sample(uint16_t x, uint16_t y, uint8_t channel) const
	{
		if (channel >= m_channels) { return 0; }

		if (x >= m_width || y >= m_height) { return 0; }

		const auto index = y * m_width * m_channels + x * m_channels + channel;

		if (index >= m_raster.size()) { return 0; }

		return m_raster[index];
	}
	T sample_relative(float x, float y, uint8_t channel) const
	{
		return sample(x * m_width, y * m_height, channel);
	}
public: // rasterize methods
	// line drawing
	void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t channel, T color)
	{
		int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
		int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
		int err = dx + dy, e2; // error value e_xy

		for (;;) {
			plot(x0, y0, channel, color);
			if (x0 == x1 && y0 == y1) { break; }
			e2 = 2 * err;
			if (e2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0
			if (e2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0
		}
	}
	void draw_line_relative(const glm::vec2 &a, const glm::vec2 &b, uint8_t channel, T color)
	{
		draw_line(a.x * m_width, a.y * m_height, b.x * m_width, b.y * m_height, channel, color);
	}
	void draw_filled_circle(uint16_t x0, uint16_t y0, int radius, uint8_t channel, T color)
	{
		int x = radius;
		int y = 0;
		int xchange = 1 - (radius << 1);
		int ychange = 0;
		int err = 0;

		while (x >= y) {
			for (int i = x0 - x; i <= x0 + x; i++) {
				plot(i, y0 + y, channel, color);
				plot(i, y0 - y, channel, color);
			}
			for (int i = x0 - y; i <= x0 + y; i++) {
				plot(i, y0 + x, channel, color);
				plot(i, y0 - x, channel, color);
			}

			y++;
			err += ychange;
			ychange += 2;
			if (((err << 1) + xchange) > 0) {
				x--;
				err += xchange;
				xchange += 2;
			}
		}
	}
	void draw_thick_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, int radius, uint8_t channel, T color)
	{
		int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
		int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
		int err = dx + dy, e2; // error value e_xy

		for (;;) {
			draw_filled_circle(x0, y0, radius, channel, color);
			if (x0 == x1 && y0 == y1) { break; }
			e2 = 2 * err;
			if (e2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0
			if (e2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0
		}
	}
	void draw_thick_line_relative(const glm::vec2 &a, const glm::vec2 &b, int radius, uint8_t channel, T color)
	{
		draw_thick_line(a.x * m_width, a.y * m_height, b.x * m_width, b.y * m_height, radius, channel, color);
	}
	// triangle rasterize
	void draw_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, uint8_t channel, T color)
	{
		// make sure the triangle is counter clockwise
		if (geom::clockwise(a, b, c)) {
			std::swap(b, c);
		}

		a.x = floorf(a.x);
		a.y = floorf(a.y);
		b.x = floorf(b.x);
		b.y = floorf(b.y);
		c.x = floorf(c.x);
		c.y = floorf(c.y);

		// this seems to fix holes
		draw_line(a.x, a.y, b.x, b.y, channel, color);
		draw_line(b.x, b.y, c.x, c.y, channel, color);
		draw_line(c.x, c.y, a.x, a.y, channel, color);

		// Compute triangle bounding box
		int minX = std::min(int(a.x), std::min(int(b.x), int(c.x)));
		int minY = std::min(int(a.y), std::min(int(b.y), int(c.y)));
		int maxX = std::max(int(a.x), std::max(int(b.x), int(c.x)));
		int maxY = std::max(int(a.y), std::max(int(b.y), int(c.y)));

		// Clip against screen bounds
		minX = (std::max)(minX, 0);
		minY = (std::max)(minY, 0);
		maxX = (std::min)(maxX, m_width - 1);
		maxY = (std::min)(maxY, m_height - 1);

		// Triangle setup
		int A01 = a.y - b.y, B01 = b.x - a.x;
		int A12 = b.y - c.y, B12 = c.x - b.x;
		int A20 = c.y - a.y, B20 = a.x - c.x;

		// Barycentric coordinates at minX/minY corner
		glm::ivec2 p = { minX, minY };
		int w0_row = geom::orient(b.x, b.y, c.x, c.y, p.x, p.y);
		int w1_row = geom::orient(c.x, c.y, a.x, a.y, p.x, p.y);
		int w2_row = geom::orient(a.x, a.y, b.x, b.y, p.x, p.y);

		// Rasterize
		for (p.y = minY; p.y <= maxY; p.y++) {
			// Barycentric coordinates at start of row
			int w0 = w0_row;
			int w1 = w1_row;
			int w2 = w2_row;

			for (p.x = minX; p.x <= maxX; p.x++) {
				// If p is on or inside all edges, render pixel.
				if ((w0 | w1 | w2) >= 0) {
					plot(p.x, p.y, channel, color);
				}

				// One step to the right
				w0 += A12;
				w1 += A20;
				w2 += A01;
			}

			// One row step
			w0_row += B12;
			w1_row += B20;
			w2_row += B01;
		}
	}
	void draw_triangle_relative(glm::vec2 a, glm::vec2 b, glm::vec2 c, uint8_t channel, T color)
	{
		glm::vec2 a_coords = { a.x * m_width, a.y * m_height };
		glm::vec2 b_coords = { b.x * m_width, b.y * m_height };
		glm::vec2 c_coords = { c.x * m_width, c.y * m_height };

		draw_triangle(a_coords, b_coords, c_coords, channel, color);
	}
public:
	void blur(float sigma);
	void normalize(uint8_t channel);
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_width, m_height, m_channels, m_raster);
	}
public:
	uint8_t channels() const { return m_channels; }
	uint16_t width() const { return m_width; }
	uint16_t height() const { return m_height; }
	const std::vector<T>& raster() const { return m_raster; }
	std::vector<T>& raster() { return m_raster; }
private:
	uint8_t m_channels = 0;
	uint16_t m_width = 0;
	uint16_t m_height = 0;
	std::vector<T> m_raster;
};

inline glm::vec3 filter_normal(int x, int y, uint8_t channel, float strength, const Image<uint8_t> &image)
{
	float T = image.sample(x, y + 1, channel) / 255.f;
	float TR = image.sample(x + 1, y + 1, channel) / 255.f;
	float TL = image.sample(x - 1, y + 1, channel) / 255.f;
	float B = image.sample(x, y - 1, channel) / 255.f;
	float BR = image.sample(x + 1, y - 1, channel) / 255.f;
	float BL = image.sample(x - 1, y - 1, channel) / 255.f;
	float R = image.sample(x + 1, y, channel) / 255.f;
	float L = image.sample(x - 1, y, channel) / 255.f;

	// sobel filter
	const float X = (TR + 2.f * R + BR) - (TL + 2.f * L + BL);
	const float Z = (BL + 2.f * B + BR) - (TL + 2.f * T + TR);
	const float Y = 1.f / strength;

	glm::vec3 normal(-X, Y, Z);

	return glm::normalize(normal);
}

inline glm::vec3 filter_normal(int x, int y, uint8_t channel, float strength, const Image<float> &image)
{
	float T = image.sample(x, y + 1, channel);
	float TR = image.sample(x + 1, y + 1, channel);
	float TL = image.sample(x - 1, y + 1, channel);
	float B = image.sample(x, y - 1, channel);
	float BR = image.sample(x + 1, y - 1, channel);
	float BL = image.sample(x - 1, y - 1, channel);
	float R = image.sample(x + 1, y, channel);
	float L = image.sample(x - 1, y, channel);

	// sobel filter
	const float X = (TR + 2.f * R + BR) - (TL + 2.f * L + BL);
	const float Z = (BL + 2.f * B + BR) - (TL + 2.f * T + TR);
	const float Y = 1.f / strength;

	glm::vec3 normal(-X, Y, Z);

	return glm::normalize(normal);
}

};

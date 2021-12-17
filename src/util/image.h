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

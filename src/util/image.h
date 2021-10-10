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

};

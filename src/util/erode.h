namespace util {

enum : uint8_t {
	LEFT = 0,
	RIGHT = 1,
	TOP = 2,
	BOTTOM = 3
};

class Eroder {
public:
	void erode(Image<float> &image);
private:
	int m_width = 0;
	int m_height = 0;
	Image<float> m_terrain_ping;
	Image<float> m_terrain_pong;
	Image<float> m_water_ping;
	Image<float> m_water_pong;
	Image<float> m_flux_ping;
	Image<float> m_flux_pong;
	Image<float> m_sediment_ping;
	Image<float> m_sediment_pong;
	Image<float> m_velocity;
private:
	void erosion_step(float time);
private:
	void increment_water(int x, int y, float time);
	void simulate_flow(int x, int y, float time);
	void update_water_velocity(int x, int y, float time);
	void erosion_deposition(int x, int y);
	void transport_sediment(int x, int y, float time);
	void evaporate_water(int x, int y, float time);
private:
	float terrain_slope(int x, int y) const;
};

};

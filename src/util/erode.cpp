#include <cstdint>
#include <vector>
#include <string>
#include <cmath>

#include "image.h"
#include "erode.h"

namespace util {

static const float l = 1.f; // pipe length
			    //
struct vec3 {
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
	float magnitude()
	{
		float sum = x * x + y * y + z * z;
		return sqrtf(sum);
	}
	void normalize()
	{
		float len = magnitude();
		if (len > 0.f) {
			x /= len;
			y /= len;
			z /= len;
		}
	}
};

static inline float output_flow(float A, float g, float l, float h, float f, float time)
{
	float s = A * ((g * h) / l);

	return std::max(0.f, f + time * s);
}
	
// find slope angle at location xy
float Eroder::terrain_slope(int x, int y) const
{
	float T = m_terrain_ping.sample(x, y + 1, CHANNEL_RED);
	float B = m_terrain_ping.sample(x, y - 1, CHANNEL_RED);
	float R = m_terrain_ping.sample(x + 1, y, CHANNEL_RED);
	float L = m_terrain_ping.sample(x - 1, y, CHANNEL_RED);

	vec3 normal = { L - R, 2.f, T - B };
	normal.normalize();

	return normal.y;
}
	
void Eroder::erode(Image<float> &image)
{
	m_width = image.width();
	m_height = image.height();

	m_terrain_ping.resize(image.width(), image.height(), 1);
	m_terrain_pong.resize(image.width(), image.height(), 1);
	m_water_ping.resize(image.width(), image.height(), 1);
	m_water_pong.resize(image.width(), image.height(), 1);
	m_flux_ping.resize(image.width(), image.height(), 4);
	m_flux_pong.resize(image.width(), image.height(), 4);
	m_velocity.resize(image.width(), image.height(), 2);
	m_sediment_ping.resize(image.width(), image.height(), 1);
	m_sediment_pong.resize(image.width(), image.height(), 1);

	m_terrain_ping.copy(image);

	float time = 0.1f;
	for (int i = 0; i < 1000; i++) {
		erosion_step(time);
	}

	image.copy(m_terrain_ping);
}
	
void Eroder::erosion_step(float time)
{
	const int n_steps = 32;
	const int step_size = m_width / n_steps;

	#pragma omp parallel for
	for (int step_x = 0; step_x < m_width; step_x += step_size) {
		int w = step_x + step_size;
		int h = m_height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				increment_water(i, j, time);
			}
		}
	}
	std::swap(m_water_ping, m_water_pong);

	#pragma omp parallel for
	for (int step_x = 0; step_x < m_width; step_x += step_size) {
		int w = step_x + step_size;
		int h = m_height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				simulate_flow(i, j, time);
			}
		}
	}

	std::swap(m_flux_ping, m_flux_pong);

	#pragma omp parallel for
	for (int step_x = 0; step_x < m_width; step_x += step_size) {
		int w = step_x + step_size;
		int h = m_height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				update_water_velocity(i, j, time);
			}
		}
	}

	std::swap(m_water_ping, m_water_pong);

	#pragma omp parallel for
	for (int step_x = 0; step_x < m_width; step_x += step_size) {
		int w = step_x + step_size;
		int h = m_height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				erosion_deposition(i, j);
			}
		}
	}

	std::swap(m_sediment_ping, m_sediment_pong);
	std::swap(m_terrain_ping, m_terrain_pong);

	#pragma omp parallel for
	for (int step_x = 0; step_x < m_width; step_x += step_size) {
		int w = step_x + step_size;
		int h = m_height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				transport_sediment(i, j, time);
			}
		}
	}

	std::swap(m_sediment_ping, m_sediment_pong);

	#pragma omp parallel for
	for (int step_x = 0; step_x < m_width; step_x += step_size) {
		int w = step_x + step_size;
		int h = m_height;
		for (int i = 0; i < h; i++) {
			for (int j = step_x; j < w; j++) {
				evaporate_water(i, j, time);
			}
		}
	}

	std::swap(m_water_ping, m_water_pong);
}
	
// add amount of water arriving in a time frame
void Eroder::increment_water(int x, int y, float time)
{
	float rain = 0.01f; // fixed amount of rain
	// update water height
	float water = m_water_ping.sample(x, y, CHANNEL_RED);
	float d = water + time * rain;
	m_water_pong.plot(x, y, CHANNEL_RED, d);
}

void Eroder::simulate_flow(int x, int y, float time)
{
	float A = l * l; // pipe area
	float g = 0.8f; // gravity
			//
	float f_left = m_flux_ping.sample(x, y, LEFT);
	float f_right = m_flux_ping.sample(x, y, RIGHT);
	float f_bottom = m_flux_ping.sample(x, y, BOTTOM);
	float f_top = m_flux_ping.sample(x, y, TOP);

	// terrain height data
	float top = m_terrain_ping.sample(x, y + 1, CHANNEL_RED);
	float bottom = m_terrain_ping.sample(x, y - 1, CHANNEL_RED);
	float right = m_terrain_ping.sample(x + 1, y, CHANNEL_RED);
	float left = m_terrain_ping.sample(x - 1, y, CHANNEL_RED);

	float b = m_terrain_ping.sample(x, y, CHANNEL_RED);

	// water height data
	float w_top = m_water_ping.sample(x, y + 1, CHANNEL_RED);
	float w_bottom = m_water_ping.sample(x, y - 1, CHANNEL_RED);
	float w_right = m_water_ping.sample(x + 1, y, CHANNEL_RED);
	float w_left = m_water_ping.sample(x - 1, y, CHANNEL_RED);

	float d = m_water_ping.sample(x, y, CHANNEL_RED);

	// height difference between this cell and neighbor cells
	float h_left = (b + d) - (left + w_left);
	float h_right = (b + d) - (right + w_right);
	float h_top = (b + d) - (top + w_top);
	float h_bottom = (b + d) - (bottom + w_bottom);

	// compute the outgoing amount of water from the current cell to the neighbors
	float f_L = output_flow(A, g, l, h_left, f_left, time);
	float f_R = output_flow(A, g, l, h_right, f_right, time);
	float f_T = output_flow(A, g, l, h_top, f_top, time);
	float f_B = output_flow(A, g, l, h_bottom, f_bottom, time);

	// scaling factor K for the outflow flux
	float K = std::min(1.f, (d * l * l) / ((f_L + f_R + f_T + f_B) * time)); 

	// scale output flux by K
	f_L *= K;
	f_R *= K;
	f_T *= K;
	f_B *= K;

	// deal with boundary conditions
	// water can't leave the map
	if (x <= 0) { f_L = 0.f; }
	if (x >= m_width - 1) { f_R = 0.f; }
	if (y <= 0) { f_B = 0.f; }
	if (y >= m_height - 1) { f_T = 0.f; }

	m_flux_pong.plot(x, y, LEFT, f_L);
	m_flux_pong.plot(x, y, RIGHT, f_R);
	m_flux_pong.plot(x, y, BOTTOM, f_B);
	m_flux_pong.plot(x, y, TOP, f_T);
}
	
void Eroder::update_water_velocity(int x, int y, float time)
{
	//float l = 1.f; // pipe length
		       //
	/* The water height is updated with the new outflow flux field
	by collecting the inflow flux fin from neighbor cells, and
	sending the outflow flux fout away from the current cell. */

	// incoming flow is the outgoing flow from neighbor cells in opposite directions
	float top_in = m_flux_ping.sample(x, y + 1, BOTTOM);
	float bottom_in = m_flux_ping.sample(x, y - 1, TOP);
	float right_in = m_flux_ping.sample(x + 1, y, LEFT);
	float left_in = m_flux_ping.sample(x - 1, y, RIGHT);

	float top_out = m_flux_ping.sample(x, y, TOP);
	float bottom_out = m_flux_ping.sample(x, y, BOTTOM);
	float right_out = m_flux_ping.sample(x, y, RIGHT);
	float left_out = m_flux_ping.sample(x, y, LEFT);

	float inflow_flux = top_in + bottom_in + right_in + left_in;
	float outflow_flux = bottom_out + top_out + left_out + right_out; 

	float volume_change = time * (inflow_flux - outflow_flux);

	// update water height
	float d1 = m_water_ping.sample(x, y, CHANNEL_RED);
	float d2 = d1 + (volume_change / (l * l));
			
	// update the velocity vector field
	// the velocity field is calculated from the outflow flux
	float W_x = 0.5f * (left_in - left_out + right_out - right_in);
	float W_y = 0.5f * (bottom_in - bottom_out + top_out - top_in);

	float da = 0.5f * (d1 + d2);

	if (d1 < 0.01f || da < 0.0001f) {
		W_x = 0.f;
		W_y = 0.f;
	}

	// deal with boundary conditions
	/*
	if (x >= m_width - 1 || x <= 0 || y >= m_height - 1 || y <= 0) {
		W_x = 0.f;
		W_y = 0.f;
	}
	*/

	m_water_pong.plot(x, y, CHANNEL_RED, d2);

	m_velocity.plot(x, y, CHANNEL_RED, W_x);
	m_velocity.plot(x, y, CHANNEL_GREEN, W_y);
}
	
// erode soil and deposit some
void Eroder::erosion_deposition(int x, int y)
{
	const float Ks = 0.025; // dissolving constant
	const float Kd = 0.025; // deposition constant
	// sediment transport capacity constant Kc
	const float Kc = 0.08F;

	float tilt_angle = terrain_slope(x, y);
	tilt_angle = abs(sqrtf(1.f - tilt_angle * tilt_angle));

	// for very flat terrains the angle will be close to 0
	// so limit it
	tilt_angle = std::max(0.1f, tilt_angle) ;

	float v_x = m_velocity.sample(x, y, CHANNEL_RED);
	float v_y = m_velocity.sample(x, y, CHANNEL_GREEN);
	float v = sqrtf(v_x * v_x + v_y * v_y);

	// sediment transport capacity C
	float C = Kc * sin(tilt_angle) * v;

	float st = m_sediment_ping.sample(x, y, CHANNEL_RED);

	// current terrain height
	float d = m_terrain_ping.sample(x, y, CHANNEL_RED);

	float s1 = 0.f;
	float d1 = 0.f;

	// compare C with the suspended sediment amount s to decide the erosion-deposition process
	// if C > st some soil is dissolved into the water and added to the suspended sediment
	if (C > st) {
		float soil = Ks * (C - st);
		d1 = d - soil; // remove soil from terrain
		s1 = st + soil; // add soil to sediment
	} else if (C <= st) {
		// deposit sediment
		float soil = Kd * (st - C);
		d1 = d + soil;
		s1 = st - soil;
	}

	// clamp height
	if (d1 > 1.f) {
		d1 = 1.f;
	} else if (d1 < 0.f) {
		d1 = 0.f;
	}

	m_terrain_pong.plot(x, y, CHANNEL_RED, d1);
	m_sediment_pong.plot(x, y, CHANNEL_RED, s1);
}
	
void Eroder::transport_sediment(int x, int y, float time)
{
	// the updated sediment is transported by the flow velocity vector
	float v_x = m_velocity.sample(x, y, CHANNEL_RED);
	float v_y = m_velocity.sample(x, y, CHANNEL_GREEN);

	// semi-Lagrangian advection method
	float back_x = x - v_x * time;
	float back_y = y - v_y * time;

	float st = m_sediment_ping.sample(back_x, back_y, CHANNEL_RED);

	m_sediment_pong.plot(x, y, CHANNEL_RED, st);
}
	
void Eroder::evaporate_water(int x, int y, float time)
{
	const float Ke = 0.4; // evaporation constant Ke
	
	float d2 = m_water_ping.sample(x, y, CHANNEL_RED);

	float dt = d2 * (1.f - Ke * time);
	
	m_water_pong.plot(x, y, CHANNEL_RED, dt);
}

};

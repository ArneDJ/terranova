#include <chrono>
#include <array>

#include "timer.h"

namespace util {

void FrameTimer::begin()
{
	m_time_stamp = std::chrono::high_resolution_clock::now();
}

void FrameTimer::finish()
{
	auto time_end = std::chrono::high_resolution_clock::now();
	auto time_diff = std::chrono::duration<double, std::milli>(time_end - m_time_stamp).count();

	m_delta = float(time_diff) / 1000.f;

	float fps_time = float((std::chrono::duration<double, std::milli>(time_end - m_last_fps_time).count()));
	if (fps_time > FPS_UPDATE_TIME) {
		m_last_FPS = static_cast<uint32_t>(float(m_fps_count) * (FPS_UPDATE_TIME / fps_time));
		m_fps_count = 0;
		m_last_fps_time = time_end;
	}

	m_fps_count++;
}

float FrameTimer::delta_seconds() const
{
	return m_delta;
}

float FrameTimer::delta_miliseconds() const
{
	return 1000.F * m_delta;
}
	
uint32_t FrameTimer::frames_per_second() const
{
	return m_last_FPS;
}

};

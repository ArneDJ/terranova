namespace util {

using TimeStamp = std::chrono::time_point<std::chrono::high_resolution_clock>;

class FrameTimer {
public:
	const float FPS_UPDATE_TIME = 1000.F;
public:
	void begin();
	void finish();
public:
	float delta_seconds() const;
	float delta_miliseconds() const;
	uint32_t frames_per_second() const;
private:
	float m_delta = 0.f; // in seconds
	uint32_t m_last_FPS = 0;
	TimeStamp m_time_stamp;
	uint32_t m_fps_count = 0;
	TimeStamp m_last_fps_time;
};

};

#pragma once
#include <queue>
#include <set>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/cereal/types/unordered_map.hpp"
#include "../extern/cereal/types/vector.hpp"
#include "../extern/cereal/types/memory.hpp"
#include "../extern/cereal/types/utility.hpp"
#include "../extern/cereal/types/queue.hpp"
#include "../extern/cereal/types/set.hpp"

namespace glm {

template<class Archive> 
void serialize(Archive &archive, glm::vec2 &v) 
{ 
	archive(
		cereal::make_nvp("x", v.x), 
		cereal::make_nvp("y", v.y) 
	); 
}

template<class Archive> 
void serialize(Archive &archive, glm::vec3 &v) 
{ 
	archive(
		cereal::make_nvp("x", v.x), 
		cereal::make_nvp("y", v.y), 
		cereal::make_nvp("z", v.z)
	); 
}

template<class Archive> 
void serialize(Archive &archive, glm::quat &q) 
{ 
	archive(
		cereal::make_nvp("x", q.x), 
		cereal::make_nvp("y", q.y), 
		cereal::make_nvp("z", q.z), 
		cereal::make_nvp("w", q.w)
	); 
}

};

namespace util {

class IdGenerator {
public:
	uint32_t generate()
	{
		uint32_t id = 0;

		// generate new id if not available
		if (m_available.empty()) {
			id = m_counter++;
		} else {
			id = m_available.front();
			m_available.pop();
		}

		m_reserved.insert(id);

		return id;
	}
	void release(uint32_t id)
	{
		auto search = m_reserved.find(id);
		if (search != m_reserved.end()) {
			m_reserved.erase(id);
			m_available.push(id);
		}
	}
	void reset()
	{
		m_reserved.clear();
		m_available = std::queue<uint32_t>();
		m_counter = 1;
	}
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_reserved, m_available, m_counter);
	}
private:
	std::set<uint32_t> m_reserved;
	std::queue<uint32_t> m_available;
	uint32_t m_counter = 1;
};

};


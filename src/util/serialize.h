#pragma once
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/cereal/types/unordered_map.hpp"
#include "../extern/cereal/types/vector.hpp"
#include "../extern/cereal/types/memory.hpp"
#include "../extern/cereal/types/utility.hpp"

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


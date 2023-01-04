#include <iostream>
#include <fstream>

#include "extern/cereal/archives/binary.hpp"
#include "extern/cereal/archives/json.hpp"

#include "util/logger.h"
#include "util/serialize.h"
#include "geometry/geometry.h"

#include "module.h"

template <class T>
void Module::load_json(T &data, const std::string &filepath)
{
	std::ifstream stream(filepath);

	if (stream.is_open()) {
		cereal::JSONInputArchive archive(stream);
		archive(data);
	} else {
		logger::ERROR("Could not load {}", filepath);
	}
}

void Module::load()
{
	load_json(architectures, "data/houses.json");
	load_json(fortification, "data/fortifications.json");
	load_json(human_armature, "data/creatures.json");
	load_json(board_module, "data/overworld.json");
}

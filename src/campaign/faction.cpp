#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "faction.h"

uint32_t Faction::id() const { return m_id; };

glm::vec3 Faction::color() const { return m_color; };

void Faction::set_color(const glm::vec3 &color)
{
	m_color = color;
}

void Faction::set_id(uint32_t id) 
{ 
	m_id = id; 
};

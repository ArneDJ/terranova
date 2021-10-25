#include <vector>
#include <memory>
#include <list>
#include <queue>
#include <GL/glew.h>
#include <GL/gl.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../geometry/geometry.h"
#include "../geometry/voronoi.h"
#include "../geometry/transform.h"
#include "../util/image.h"
#include "../graphics/mesh.h"
#include "../graphics/texture.h"
#include "../graphics/model.h"

#include "marker.h"
	
Marker::Marker()
{
	m_transform = std::make_unique<geom::Transform>();
}

void Marker::teleport(const glm::vec3 &position)
{
	m_transform->position = position;
	m_visible = true;
}
	
bool Marker::visible() const
{
	return m_visible;
}
	
const geom::Transform* Marker::transform() const
{
	return m_transform.get();
}
	
void Marker::set_model(const gfx::Model *model)
{
	m_model = model;
}
	
const gfx::Model* Marker::model() const
{
	return m_model;
}

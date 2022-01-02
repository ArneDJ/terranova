#pragma once

namespace fysx {

class TriggerSphere {
public:
	TriggerSphere(const geom::Sphere &form);
public:
	const geom::Transform* transform() const {  return m_transform.get(); }
	btGhostObject* ghost_object() const { return m_ghost_object.get(); }
	glm::vec3 position() const;
	float radius() const;
public:
	void set_position(const glm::vec3 &position);
private:
	std::unique_ptr<btGhostObject> m_ghost_object;
	std::unique_ptr<btSphereShape> m_shape;
	std::unique_ptr<geom::Transform> m_transform;
};

};

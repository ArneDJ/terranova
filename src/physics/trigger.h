#pragma once

namespace fysx {

class TriggerSphere {
public:
	TriggerSphere(const geom::Sphere &form);
public:
	const geom::Sphere& form() const { return m_form; }
	const geom::Transform* transform() const {  return m_transform.get(); }
	float radius() const { return m_form.radius; }
public:
	void set_position(const glm::vec3 &position) 
	{
		m_ghost_object->getWorldTransform().setOrigin(vec3_to_bt(position));
		m_transform->position = position;
	}
private:
	geom::Sphere m_form; // TODO remove this
	std::unique_ptr<btGhostObject> m_ghost_object;
	std::unique_ptr<btSphereShape> m_shape;
	std::unique_ptr<geom::Transform> m_transform;
};

};

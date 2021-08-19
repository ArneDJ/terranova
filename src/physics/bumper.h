namespace fysx {

class Bumper {
public:
	Bumper(const glm::vec3 &origin, float radius, float length);
public:
	void update(const btDynamicsWorld *world);
	void jump();
	void set_velocity(float x, float z);
public:
	const geom::Transform* transform() const;
	btRigidBody* body() const;
	glm::vec3 standing_position() const;
	bool grounded() const;
	float capsule_radius() { return m_capsule_radius; }
	float capsule_height() { return m_capsule_height; }
private:
	std::unique_ptr<btRigidBody> m_body;
	std::unique_ptr<btCollisionShape> m_shape;
	std::unique_ptr<btMotionState> m_motion;
	std::unique_ptr<geom::Transform> m_transform;
	glm::vec3 m_velocity = {};
	bool m_grounded = false;
	float m_probe_ground = 0.f;
	float m_probe_air = 0.f;
	float m_height = 1.f;
	float m_offset = 0.f;
	float m_body_offset = 0.f;
	float m_capsule_radius = 0.f;
	float m_capsule_height = 0.f;
};

};

namespace fysx {

class Bumper {
public:
	std::unique_ptr<geom::Transform> transform;
	std::unique_ptr<btConvexShape> shape;
	std::unique_ptr<btPairCachingGhostObject> ghost_object;
	//std::unique_ptr<btRigidBody> body;
	//std::unique_ptr<btMotionState> motion;
	glm::vec3 walk_direction = {};
	float speed = 10.f;
	bool grounded = false;
	bool on_high_slope = false;
	btVector3 slope_normal;
	float step_height = 0.05f;
public:
	Bumper(const glm::vec3 &origin, float radius, float length);
public:
	void update(btDynamicsWorld *world, float delta);
	void teleport(const glm::vec3 &position);
	void sync_transform();
	void step_forward(btDynamicsWorld *world, const glm::vec3 &velocity);
	void collide_and_slide(btDynamicsWorld *world, const glm::vec3 &velocity, float delta);
};

};

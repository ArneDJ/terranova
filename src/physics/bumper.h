namespace fysx {

class Bumper {
public:
	std::unique_ptr<geom::Transform> transform;
	std::unique_ptr<btConvexShape> shape;
	std::unique_ptr<btPairCachingGhostObject> ghost_object;
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
	void apply_velocity(const glm::vec3 &velocity);
	void sync_transform();
	glm::vec3 collide_and_slide(const btDynamicsWorld *world, const glm::vec3 &velocity, float delta);
	glm::vec3 local_push(btDynamicsWorld *world, float delta, float speed);
	glm::vec3 collide_and_block(const btDynamicsWorld *world, const glm::vec3 &velocity);
	void stick_to_floor(const btDynamicsWorld *world);
	void try_move(const btDynamicsWorld *world, const glm::vec3 &velocity, float delta);
};

};

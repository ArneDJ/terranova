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
	void sync_transform();
	glm::vec3 collide_and_slide(btDynamicsWorld *world, const glm::vec3 &velocity);
	void collide_and_move(btDynamicsWorld *world, float delta, float speed, const glm::vec3 &velocity);
	glm::vec3 collide_and_block(btDynamicsWorld *world, const glm::vec3 &velocity);
};

};

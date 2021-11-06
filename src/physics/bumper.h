namespace fysx {

class Bumper {
public:
	std::unique_ptr<geom::Transform> transform;
	std::unique_ptr<btConvexShape> shape;
	std::unique_ptr<btPairCachingGhostObject> ghost_object;
	glm::vec3 walk_direction = {};
	float speed = 10.f;
	bool grounded = false;
public:
	Bumper(const glm::vec3 &origin, float radius, float length);
public:
	void update(btDynamicsWorld *world, float delta);
	void teleport(const glm::vec3 &position);
	void apply_velocity(const glm::vec3 &velocity);
	void sync_transform();
private:
	glm::vec3 collide_and_slide(const btDynamicsWorld *world, const glm::vec3 &velocity, float delta);
	void stick_to_floor(const btDynamicsWorld *world);
	void slide_move(const btDynamicsWorld *world, const glm::vec3 &displacement, float delta);
	void collide_n_slide(const btDynamicsWorld *world, const glm::vec3 &displacement);
};

};

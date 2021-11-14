namespace fysx {

class Bumper {
public:
	std::unique_ptr<geom::Transform> transform;
	std::unique_ptr<btCapsuleShape> shape;
	std::unique_ptr<btPairCachingGhostObject> ghost_object;
	glm::vec3 walk_direction = {};
public:
	float speed = 8.f;
	bool on_ground = false;
	float fallen_distance = 0.f; // also used to calculate falling damage
public:
	Bumper(const glm::vec3 &origin, float radius, float length);
public:
	void update(const btDynamicsWorld *world, float delta);
	void teleport(const glm::vec3 &position);
	void apply_velocity(const glm::vec3 &velocity);
	void sync_transform();
	void set_scale(float scale);
private:
	void apply_gravity(const btDynamicsWorld *world, float delta);
	void collide_and_slide(const btDynamicsWorld *world, const glm::vec3 &displacement);
	void update_fallen_distance(const glm::vec3 &start, const glm::vec3 &end);
};

};

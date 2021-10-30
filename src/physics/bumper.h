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
	void update(const btDynamicsWorld *world, float delta);
	void teleport(const glm::vec3 &position);
	void sync_transform();
};

};

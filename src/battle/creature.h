
class Creature {
public:
	std::unique_ptr<fysx::Bumper> bumper;
	std::unique_ptr<geom::Transform> transform;
public: // animation stuff
	const gfx::Model *model = nullptr;
	gfx::BufferDataPair<glm::mat4> joint_matrices;
	util::CharacterAnimation character_animation;
public:
	Creature();
	void teleport(const glm::vec3 &position);
	void set_animation(const ozz::animation::Skeleton *skeleton, const ozz::animation::Animation *animation);
	void set_movement(const glm::vec3 &direction, bool jump_request);
	void update_collision(const btDynamicsWorld *world, float delta);
	void update_transform();
	void update_animation(const ozz::animation::Skeleton *skeleton, const ozz::animation::Animation *animation, float delta);
};

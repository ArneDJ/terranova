
enum CreatureAnimation : uint8_t {
	CA_IDLE = 1,
	CA_RUN,
	CA_FALLING
};

struct CreatureSkeletonAttachments {
	int eyes = -1;
	int left_hand = -1;
	int right_hand = -1;
};

class Creature {
public:
	std::unique_ptr<fysx::Bumper> bumper;
	std::unique_ptr<geom::Transform> transform;
	glm::vec3 eye_position = {};
public: // animation stuff
	const gfx::Model *model = nullptr;
	util::AnimationSet *anim_set = nullptr;
	CreatureSkeletonAttachments skeleton_attachments;
	gfx::BufferDataPair<glm::mat4> joint_matrices;
	util::CharacterAnimation character_animation;
	CreatureAnimation current_animation = CA_IDLE;
public:
	Creature();
public:
	void teleport(const glm::vec3 &position);
	void set_animation(util::AnimationSet *set);
	void set_movement(const glm::vec3 &direction, bool jump_request);
	void update_collision(const btDynamicsWorld *world, float delta);
	void update_transform();
	void update_animation(float delta);
public:
	void set_scale(float scale);
};

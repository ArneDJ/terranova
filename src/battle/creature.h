
enum CreatureAnimation : uint8_t {
	CA_IDLE = 1,
	CA_RUN,
	CA_FALLING
};

struct CreatureSkeletonAttachments {
	int eyes = -1;
	int head = -1;
	int spine = -1;
	int upper_right_arm = -1;
	int upper_left_arm = -1;
	int lower_right_arm = -1;
	int lower_left_arm = -1;
	int left_hand = -1;
	int right_hand = -1;
};

struct HitBoxInput {
	std::string joint_a = "";
	std::string joint_b = "";
	float radius = 1.f;
	float half_height = 1.f;
};

class HitBox {
public:
	std::unique_ptr<btCapsuleShape> shape;
	std::unique_ptr<btPairCachingGhostObject> ghost_object;
	std::unique_ptr<geom::Transform> transform;
	int joint_target_a = -1; // follows the animation
	int joint_target_b = -1; // follows the animation
public:
	HitBox(float radius, float half_height)
	{
		shape = std::make_unique<btCapsuleShape>(radius, half_height);

		btTransform t;
		t.setIdentity();

		ghost_object = std::make_unique<btPairCachingGhostObject>();
		ghost_object->setWorldTransform(t);
		ghost_object->setCollisionShape(shape.get());

		transform = std::make_unique<geom::Transform>();
	}
	void sync_transform()
	{
		btTransform t = ghost_object->getWorldTransform();

		transform->position = fysx::bt_to_vec3(t.getOrigin());
		transform->rotation = fysx::bt_to_quat(t.getRotation());
	}
	void set_transform(const glm::vec3 &position, const glm::quat &rotation) 
	{
		transform->position = position;
		transform->rotation = rotation;

		btTransform t;
		t.setIdentity ();
		t.setOrigin(fysx::vec3_to_bt(position));
		t.setRotation(fysx::quat_to_bt(rotation));

		ghost_object->setWorldTransform(t);
	}
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
	std::vector<std::unique_ptr<HitBox>> hitboxes;
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

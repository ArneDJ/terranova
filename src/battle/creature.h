
enum CreatureAnimation : uint8_t {
	CA_IDLE = 1,
	CA_RUN,
	CA_BACKWARD_RUN,
	CA_STRAFE_RIGHT,
	CA_STRAFE_LEFT,
	CA_DIAGONAL_RIGHT,
	CA_DIAGONAL_LEFT,
	CA_FALLING,
	CA_ATTACK_PUNCH,
	CA_DYING,
	CA_DANCING
};

struct CreatureSkeletonAttachments {
	int eyes = -1;
	int left_hand = -1;
	int right_hand = -1;
	int spine = -1;
};

class HitBoxRoot {
public:
	std::unique_ptr<btSphereShape> shape;
	std::unique_ptr<btPairCachingGhostObject> ghost_object;
	std::unique_ptr<geom::Transform> transform;
public:
	HitBoxRoot(float radius)
	{
		shape = std::make_unique<btSphereShape>(radius);

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
	void set_position(const glm::vec3 &position) 
	{
		transform->position = position;

		btTransform t;
		t.setIdentity ();
		t.setOrigin(fysx::vec3_to_bt(position));

		ghost_object->setWorldTransform(t);
	}
	void set_scale(float scale)
	{
		shape->setLocalScaling(btVector3(scale, scale, scale));
	}
};

struct HitCapsule {
	geom::Capsule capsule = {};
	int joint_target_a = -1; // follows the animation
	int joint_target_b = -1; // follows the animation
};

class Creature {
public:
	std::unique_ptr<fysx::Bumper> bumper;
	std::unique_ptr<geom::Transform> transform;
	std::unique_ptr<geom::Transform> model_transform; // for the animated 3d model
	glm::vec3 eye_position = {};
	glm::vec3 look_direction = {};
	glm::mat4 right_hand_transform = {};
	float unit_scale = 1.f;
public: // animation stuff
	const gfx::Model *model = nullptr;
	util::AnimationSet *anim_set = nullptr;
	CreatureSkeletonAttachments skeleton_attachments;
	gfx::BufferDataPair<glm::mat4> joint_matrices;
	util::CharacterAnimation animation_sampler_a;
	util::CharacterAnimation animation_sampler_b;
	CreatureAnimation upper_body_animation = CA_IDLE;
	CreatureAnimation lower_body_animation = CA_IDLE;
	CreatureAnimation prev_lower_body_animation = CA_IDLE;
	float animation_mix = 0.f;
	float animation_blend_speed = 4.f;
public:
	std::vector<HitCapsule> hitboxes;
	// the root hitbox, this encompasses all the other hitboxes
	// when checking for a ray hitbox collision it first has to hit the root hitbox
	// before iterating over the individual hitboxes
	std::unique_ptr<HitBoxRoot> root_hitbox;
	// fist hitboxes
	std::unique_ptr<HitBoxRoot> left_fist;
public: // maybe use flags instead of bools for this?
	bool attacking = false;
	bool running = false;
	bool dead = false;
public:
	Creature();
public:
	void set_animation(util::AnimationSet *set);
	void set_hitbox(const std::vector<HitCapsule> &capsules);
public:
	void set_movement(const glm::vec3 &direction);
	void set_leg_movement(bool forward, bool backward, bool left, bool right);
	void teleport(const glm::vec3 &position);
	void update_collision(const btDynamicsWorld *world, float delta);
	void update_transform();
	void update_animation(float delta);
	void update_hitboxes();
public:
	void attack_request();
	void kill();
public:
	void set_scale(float scale);
private:
	void change_lower_body_animation(CreatureAnimation anim);
};

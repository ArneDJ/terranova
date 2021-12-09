
struct BuildingModule {
	std::string id;
	std::string model;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(id), CEREAL_NVP(model));
	}
};

struct ArchitectureStyleModule {
	geom::Bounding<uint8_t> precipitation;
	geom::Bounding<uint8_t> temperature;
	std::vector<BuildingModule> houses;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(precipitation),
			CEREAL_NVP(temperature),
			CEREAL_NVP(houses)
		);
	}
};

struct FortificationModule {
	BuildingModule segment_even;
	BuildingModule segment_both;
	BuildingModule segment_left;
	BuildingModule segment_right;
	BuildingModule tower;
	BuildingModule ramp;
	BuildingModule gate;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(segment_even),
			CEREAL_NVP(segment_both),
			CEREAL_NVP(segment_left),
			CEREAL_NVP(segment_right),
			CEREAL_NVP(tower),
			CEREAL_NVP(ramp),
			CEREAL_NVP(gate)
		);
	}
};

// binds an animation to an action code
struct AnimationActionModule {
	std::string animation = "";
	uint8_t action_code = 0;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(animation), CEREAL_NVP(action_code));
	}
};

struct HitCapsuleModule {
	std::string joint_a = "";
	std::string joint_b = "";
	float radius = 1.f;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(joint_a),
			CEREAL_NVP(joint_b),
			CEREAL_NVP(radius)
		);
	}
};

// joint_a and joint_b are needed to transition from an animation to an initial ragdoll
struct RagdollBoneModule {
	std::string joint_a = "";
	std::string joint_b = "";
	std::vector<std::string> target_joints;
	float radius = 1.f;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(joint_a),
			CEREAL_NVP(joint_b),
			CEREAL_NVP(target_joints),
			CEREAL_NVP(radius)
		);
	}
};

// hinge joint that connects two bones
struct RagdollHingeModule {
	uint32_t parent_bone = 0;
	uint32_t child_bone = 0;
	glm::vec2 limit;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(parent_bone),
			CEREAL_NVP(child_bone),
			CEREAL_NVP(limit)
		);
	}
};

// contains animation, skeleton, hitbox and ragdoll data for a creature
struct CreatureArmatureModule {
	std::string skeleton = "";
	std::vector<AnimationActionModule> animations;
	std::vector<HitCapsuleModule> hitboxes;
	// ragdoll
	std::vector<RagdollBoneModule> bones;
	std::vector<RagdollHingeModule> hinges;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(
			CEREAL_NVP(skeleton),
			CEREAL_NVP(animations),
			CEREAL_NVP(hitboxes),
			CEREAL_NVP(bones),
			CEREAL_NVP(hinges)
		);
	}
};

struct BoardModule {
	std::string marker = {};
	std::string town = {};
	std::string meeple = {};

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(marker), CEREAL_NVP(town), CEREAL_NVP(meeple));
	}
};

class Module {
public:
	BoardModule board_module;
	std::vector<ArchitectureStyleModule> architectures;
	FortificationModule fortification;
	CreatureArmatureModule human_armature;
public:
	void load();
private:
	template <class T>
	void load_json(T &data, const std::string &filepath);
};

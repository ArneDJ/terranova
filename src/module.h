
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

struct OverworldMaterial {
	std::string shader_name = "";
	std::string texture_path = "";

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(shader_name), CEREAL_NVP(texture_path));
	}
};

struct OverworldTownModule {
	uint8_t size = 0;
	float label_scale = 1.f;
	std::string base_model = {};
	std::string wall_model = {};

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(size), CEREAL_NVP(label_scale), CEREAL_NVP(base_model), CEREAL_NVP(wall_model));
	}
};

struct BoardModule {
	std::string meeple = {};
	std::string meeple_skeleton = {};
	std::string meeple_anim_idle = {};
	std::string meeple_anim_run = {};

	std::vector<OverworldTownModule> towns;

	std::vector<OverworldMaterial> materials;

	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(CEREAL_NVP(meeple), CEREAL_NVP(meeple_skeleton), CEREAL_NVP(meeple_anim_idle), CEREAL_NVP(meeple_anim_run), CEREAL_NVP(towns),  CEREAL_NVP(materials)
		);
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

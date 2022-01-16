
enum class PathState {
	FINISHED,
	MOVING,
	NEXT_NODE
};

// navigation for meeples on the campaign map
class PathFinder {
public:
	void set_nodes(const std::list<glm::vec2> &nondes);
	void update(float delta, float speed);
	void teleport(const glm::vec2 &position);
	void clear_path();
public:
	glm::vec2 location() const;
	glm::vec2 destination() const;
	glm::vec2 velocity() const;
	PathState state() const;
private:
	std::list<glm::vec2> m_nodes;
	glm::vec2 m_location;
	glm::vec2 m_destination;
	glm::vec2 m_origin;
	glm::vec2 m_velocity;
	float m_radius = 0.f;
	PathState m_state = PathState::FINISHED;
};

enum MeepleAnimation : uint8_t {
	MA_IDLE = 1,
	MA_RUN
};

enum class MeepleControlType : uint8_t {
	NONE, // not active
	PLAYER,
	AI_KINGDOM,
	AI_BANDIT,
	AI_RAIDER,
	AI_BARBARIAN
};

enum class MeepleBehavior : uint8_t {
	PATROL,
	EVADE,
	ATTACK
};

// moves on the campaign map
// either controlled by the player or the AI
class Meeple : public CampaignEntity {
public:
	MeepleControlType control_type = MeepleControlType::NONE;
	MeepleBehavior behavior_state = MeepleBehavior::PATROL;
	uint32_t target_id = 0; // id of target entity
	uint8_t target_type = 0;
	uint32_t faction_id = 0;
public:
	bool moving = false;
	uint32_t troop_count = 1; // including the leader
public:
	Meeple();
	void set_animation(const util::AnimationSet *set);
public:
	void update(float delta);
	void update_animation(float delta);
	void teleport(const glm::vec2 &position);
	void sync();
public:
	void display() const;
public:
	void set_speed(float speed);
	void set_path(const std::list<glm::vec2> &nodes);
	void clear_path();
	void clear_target();
	void set_vertical_offset(float offset);
public:
	const fysx::TriggerSphere* trigger() const;
	const fysx::TriggerSphere* visibility() const;
	glm::vec2 map_position() const;
	PathState path_state() const;
public:
	// TODO remove this and create seperate save record from entity
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(id, control_type, m_speed, transform.position, transform.rotation, transform.scale, target_id, target_type, faction_id, ticks, behavior_state,
			troop_count
		);
	}
private:
	float m_speed = 5.f;
	PathFinder m_path_finder;
private:
	MeepleAnimation m_animation = MA_IDLE;
	std::unique_ptr<util::AnimationController> m_animation_controller;
	gfx::BufferDataPair<glm::mat4> m_joint_matrices;
private:
	std::unique_ptr<fysx::TriggerSphere> m_trigger;
	std::unique_ptr<fysx::TriggerSphere> m_visibility;
};

class MeepleController {
public:
	Meeple *player = nullptr;
	std::unordered_map<uint32_t, std::unique_ptr<Meeple>> meeples;
public:
	void update(float delta);
	void add_ticks(uint64_t ticks);
	void clear();
public:
	void check_visibility(); // check if other armies are visible to player
};


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

enum class MeepleState {
	IDLING,
	ROAMING,
	TRAVELING,
	FLEEING,
	TRACKING
};

// moves on the campaign map
// either controlled by the player or the AI
class Meeple {
public:
	Meeple();
public:
	void update(float delta);
	void teleport(const glm::vec2 &position);
	void sync();
public:
	void set_id(uint32_t id);
	void set_speed(float speed);
	void set_path(const std::list<glm::vec2> &nodes);
	void set_state(MeepleState state) { m_state = state; }
	void set_vertical_offset(float offset);
public:
	void add_troops(uint32_t troop_type, int count);
public:
	uint32_t id() const;
	const geom::Transform* transform() const;
	const fysx::TriggerSphere* trigger() const;
	const fysx::TriggerSphere* visibility() const;
	glm::vec2 position() const;
	MeepleState state() const { return m_state; }
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_id, m_speed, m_transform->position, m_transform->rotation, m_transform->scale);
	}
private:
	uint32_t m_id = 0;
	float m_speed = 10.f;
	MeepleState m_state = MeepleState::ROAMING;
	PathFinder m_path_finder;
	std::unique_ptr<geom::Transform> m_transform;
private:
	std::unique_ptr<fysx::TriggerSphere> m_trigger;
	std::unique_ptr<fysx::TriggerSphere> m_visibility;
};

struct MeepleChase {
	Meeple *chaser = nullptr;
	Meeple *target = nullptr;
	bool finished = false;
};

class MeepleController {
public:
	MeepleController();
public:
	std::unique_ptr<Meeple> player;
	std::vector<std::unique_ptr<Meeple>> meeples;
public:
	std::list<std::unique_ptr<MeepleChase>> chases;
	std::list<std::unique_ptr<MeepleChase>>::iterator current_chase;
	float chase_time_slice = 0.f;
public:
	void add_chase(Meeple *chaser, Meeple *target);
	void update(float delta);
	void clear();
	void remove_meeple(Meeple *meeple);
};

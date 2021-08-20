
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
	void set_name(const std::string &name);
	void set_speed(float speed);
	void set_path(const std::list<glm::vec2> &nodes);
public:
	void add_troops(uint32_t troop_type, int count);
public:
	const geom::Transform* transform() const;
public:
	template <class Archive>
	void serialize(Archive &archive)
	{
		archive(m_name, m_speed, m_transform->position, m_transform->rotation, m_transform->scale);
	}
private:
	std::string m_name = {};
	float m_speed = 1.f;
	PathFinder m_path_finder;
	std::unique_ptr<geom::Transform> m_transform;
private:
	std::unordered_map<uint32_t, int> m_troops;
};

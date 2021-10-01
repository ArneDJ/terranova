
class Settlement {
public:
	Settlement();
public:
	uint32_t faction() const;
public:
	void set_position(const glm::vec3 &position);
	void set_faction(uint32_t faction);
public:
	const geom::Transform* transform() const;
private:
	std::unique_ptr<geom::Transform> m_transform;
	uint32_t m_faction = 0;
};

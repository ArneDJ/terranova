
class Settlement {
public:
	Settlement();
public:
	void set_position(const glm::vec3 &position);
public:
	const geom::Transform* transform() const;
private:
	std::unique_ptr<geom::Transform> m_transform;
};


// game entity with animation that shows where the mouse ray hits the campaign map
class Marker {
public:
	Marker();
public:
	void teleport(const glm::vec3 &position);
public:
	bool visible() const;
	const geom::Transform* transform() const;
private:
	bool m_visible = false;
	std::unique_ptr<geom::Transform> m_transform;
};
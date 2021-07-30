
class Campaign {
public:
	util::Camera camera;
	fysx::PhysicalSystem physics;
	std::unique_ptr<WorldMap> world;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<geom::Transform> marker;
public:
	void load();
	void save();
public:
	void init(const gfx::Shader *visual, const gfx::Shader *culling);
	void update(float delta);
	void display();
};

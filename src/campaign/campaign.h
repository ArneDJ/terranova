
class Campaign {
public:
	std::string name = {};
	util::Camera camera;
	fysx::PhysicalSystem physics;
	std::unique_ptr<WorldMap> world;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<geom::Transform> marker;
public:
	void load(const std::string &filepath);
	void save(const std::string &filepath);
public:
	void init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tilemap);
	void update(float delta);
	void display();
};

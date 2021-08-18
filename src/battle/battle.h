
class Battle {
public:
	util::Camera camera;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<geom::Transform> transform;
public:
	void init(const gfx::Shader *visual, const gfx::Shader *culling);
	void update(float delta);
	void display();
};


#include "terrain.h"

class Battle {
public:
	util::Camera camera;
	std::unique_ptr<gfx::SceneGroup> scene;
	std::unique_ptr<geom::Transform> transform;
	std::unique_ptr<Terrain> terrain;
	std::unique_ptr<Debugger> debugger;
	fysx::PhysicalSystem physics;
	std::unique_ptr<fysx::Bumper> player;
public:
	void init(const gfx::Shader *visual, const gfx::Shader *culling, const gfx::Shader *tesselation);
	void prepare(int seed);
	void update(float delta);
	void display();
};
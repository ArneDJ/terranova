#include <iostream>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "extern/loguru/loguru.hpp"

#include "extern/imgui/imgui.h"
#include "extern/imgui/imgui_impl_sdl.h"
#include "extern/imgui/imgui_impl_opengl3.h"

#include "engine.h"

class PropEntity {
public:
	PropEntity(const glm::vec3 &origin)
	{
		m_shape = std::make_unique<btBoxShape>(btVector3(1,1,1));

		/// Create Dynamic Objects
		btTransform startTransform;
		startTransform.setIdentity();

		btScalar mass(1.f);

		//rigidbody is dynamic if and only if mass is non zero, otherwise static
		bool isDynamic = (mass != 0.f);

		btVector3 localInertia(0, 0, 0);
		if (isDynamic) {
			m_shape->calculateLocalInertia(mass, localInertia);
		}

		startTransform.setOrigin(btVector3(origin.x, origin.y, origin.z));

		m_motion = std::make_unique<btDefaultMotionState>(startTransform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, m_motion.get(), m_shape.get(), localInertia);
		m_body = std::make_unique<btRigidBody>(rbInfo);

		m_transform = std::make_unique<geom::Transform>();
		m_transform->position = origin;
	}
public:
	btRigidBody* body()
	{
		return m_body.get();
	}
	void update()
	{
		btTransform transform;
		m_motion->getWorldTransform(transform);

		m_transform->position = fysx::bt_to_vec3(transform.getOrigin());
		m_transform->rotation = fysx::bt_to_quat(transform.getRotation());
	}
public:
	const geom::Transform* transform()
	{
		return m_transform.get();
	}
private:
	std::unique_ptr<btDefaultMotionState> m_motion;
	std::unique_ptr<btCollisionShape> m_shape;
	std::unique_ptr<btRigidBody> m_body;
private:
	std::unique_ptr<geom::Transform> m_transform;
};

class FixedEntity {
public:
	FixedEntity()
	{
		m_shape = std::make_unique<btStaticPlaneShape>(btVector3(0.f, 1.f, 0.f), 0.f);

		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(btVector3(0, 0, 0));

		btScalar mass(0.);

		btVector3 inertia(0, 0, 0);

		//using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
		m_motion = std::make_unique<btDefaultMotionState>(transform);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, m_motion.get(), m_shape.get(), inertia);
		m_body = std::make_unique<btRigidBody>(rbInfo);

		m_transform = std::make_unique<geom::Transform>();
		m_transform->position = glm::vec3(0.f, 0.f, 0.f);
	}
public:
	btRigidBody* body()
	{
		return m_body.get();
	}
public:
	const geom::Transform* transform()
	{
		return m_transform.get();
	}
private:
	std::unique_ptr<btDefaultMotionState> m_motion;
	std::unique_ptr<btCollisionShape> m_shape;
	std::unique_ptr<btRigidBody> m_body;
private:
	std::unique_ptr<geom::Transform> m_transform;
};

static double g_elapsed = 0.0;
static bool g_freeze_frustum = false;

static const char *GAME_NAME = "terranova";

bool UserDirectory::locate(const char *base)
{
	return (locate_dir(base, "settings", settings) && locate_dir(base, "saves", saves));
}

bool UserDirectory::locate_dir(const char *base, const char *target, std::string &output)
{
	char *pref_path = SDL_GetPrefPath(base, target);
	if (pref_path) {
		output = pref_path;

		SDL_free(pref_path);

		return true;
	}

	return false;
}

Engine::Engine()
{
	SDL_Init(SDL_INIT_VIDEO);

	// get user paths
	if (!user_dir.locate(GAME_NAME)) {
		LOG_F(FATAL, "Could not find user settings directory");
		exit(EXIT_FAILURE);
	}
	
	// load user settings
	std::string settings_filepath = user_dir.settings + "config.ini";
	if (!config.load(settings_filepath)) {
		if (config.load("default.ini")) {
			// save default settings to user path
			if (!config.save(settings_filepath)) {
				LOG_F(ERROR, "Could not save user settings %s", settings_filepath.c_str());
			}
		} else {
			LOG_F(ERROR, "Could not open default settings!");
		}
	}

	video_settings.canvas.x = config.get_integer("video", "window_width", 640);
	video_settings.canvas.y = config.get_integer("video", "window_height", 480);
	video_settings.fov = config.get_real("video", "fov", 90.0);
	
	window = SDL_CreateWindow(GAME_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, video_settings.canvas.x, video_settings.canvas.y, SDL_WINDOW_OPENGL);
	// Check that the window was successfully created
	if (window == nullptr) {
		// In the case that the window could not be made...
		LOG_F(FATAL, "Could not create window: %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (config.get_boolean("video", "fullscreen", false)) {
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
	}

	init_opengl();

	init_imgui();
}

Engine::~Engine()
{
	// in reverse order of initialization
	SDL_GL_DeleteContext(glcontext);
	// Close and destroy the window
	SDL_DestroyWindow(window);

	// Clean up SDL
	SDL_Quit();
}
	
void Engine::init_opengl()
{
	// initialize the OpenGL context
	glcontext = SDL_GL_CreateContext(window);

	GLenum error = glewInit();
	if (error != GLEW_OK) {
		LOG_F(FATAL, "Could not init GLEW: %s\n", glewGetErrorString(error));
		exit(EXIT_FAILURE);
	}

	glClearColor(0.5f, 0.5f, 0.5f, 1.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glEnable(GL_DEPTH_TEST);
	glLineWidth(2.f);
	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(-1.0f, -1.0f);
}
	
void Engine::init_imgui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(window, glcontext);
	ImGui_ImplOpenGL3_Init("#version 460");
}
	
void Engine::update_state()
{
	util::InputManager::update();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(window);
	ImGui::NewFrame();
	ImGui::Begin("Debug Mode");
	ImGui::SetWindowSize(ImVec2(400, 300));
	ImGui::Text("cam position: %f, %f, %f", camera.position.x, camera.position.y, camera.position.z);
	ImGui::Text("%.2f ms/frame (%.1d fps)", (frame_timer.FPS_UPDATE_TIME / frame_timer.frames_per_second()), frame_timer.frames_per_second());
	ImGui::Text("%.4f frame delta", frame_timer.delta_seconds());
	ImGui::Text("%f elapsed cull time", g_elapsed);
	if (ImGui::Button("Freeze Frustum")) { g_freeze_frustum = !g_freeze_frustum; }
	if (ImGui::Button("Exit")) { state = EngineState::EXIT; }
	ImGui::End();

	// update camera
	float modifier = 10.f * frame_timer.delta_seconds();
	if (util::InputManager::key_down(SDL_BUTTON_RIGHT)) {
		glm::vec2 offset = modifier * 0.05f * util::InputManager::rel_mouse_coords();
		camera.add_offset(offset);
	}
	if (util::InputManager::key_down(SDLK_w)) { camera.move_forward(modifier); }
	if (util::InputManager::key_down(SDLK_s)) { camera.move_backward(modifier); }
	if (util::InputManager::key_down(SDLK_d)) { camera.move_right(modifier); }
	if (util::InputManager::key_down(SDLK_a)) { camera.move_left(modifier); }

	camera.update_viewing();
}

void Engine::run()
{
	state = EngineState::TITLE;

	float ratio = float(video_settings.canvas.x) / float(video_settings.canvas.y);
	camera.set_projection(video_settings.fov, ratio, 0.1f, 900.f);
	camera.teleport(glm::vec3(10.f));
	camera.target(glm::vec3(0.f));

	gfx::Shader shader;
	shader.compile("shaders/triangle.vert", GL_VERTEX_SHADER);
	shader.compile("shaders/triangle.frag", GL_FRAGMENT_SHADER);
	shader.link();

	gfx::Shader culler;
	culler.compile("shaders/culling.comp", GL_COMPUTE_SHADER);
	culler.link();

	gfx::Model plane_model = gfx::Model("media/models/primitives/plane.glb");
	gfx::Model sphere_model = gfx::Model("media/models/primitives/icosphere.glb");
	gfx::Model cube_model = gfx::Model("media/models/primitives/cube.glb");
	gfx::Model teapot_model = gfx::Model("media/models/teapot.glb");
	gfx::Model dragon_model = gfx::Model("media/models/dragon.glb");

	gfx::SceneGroup scene = gfx::SceneGroup(&shader, &culler);

	Debugger debugger = Debugger(&shader, &culler);

	auto plane_object = scene.find_object(&plane_model);
	auto sphere_object = scene.find_object(&sphere_model);
	auto cube_object = scene.find_object(&cube_model);
	auto teapot_object = scene.find_object(&teapot_model);
	auto dragon_object = scene.find_object(&dragon_model);

	std::vector<std::unique_ptr<geom::Transform>> transforms;

	for (int i = 0; i < 10; i++) {
		auto transform = std::make_unique<geom::Transform>();
		transform->position = glm::vec3(float(10*i), 10.f, -10.f);
		transform->scale = glm::vec3(0.2f);
		dragon_object->add_transform(transform.get());
		debugger.add_sphere(AABB_to_sphere(dragon_model.bounds()), transform.get());
		transforms.push_back(std::move(transform));
	}

	FixedEntity plane;
	physics.add_body(plane.body());
	plane_object->add_transform(plane.transform());

	std::vector<std::unique_ptr<PropEntity>> cubes;
	for (int i = 0; i < 10; i++) {
		auto cube_prop = std::make_unique<PropEntity>(glm::vec3(-10.f, 10.f + (5*i), -10.f));
		physics.add_body(cube_prop->body());
						
		geom::AABB bounds = {
			glm::vec3(-1.f),
			glm::vec3(1.f)
		};

		debugger.add_cube(bounds, cube_prop->transform());
		sphere_object->add_transform(cube_prop->transform());
		cubes.push_back(std::move(cube_prop));
	}

	while (state == EngineState::TITLE) {
		frame_timer.begin();
	
		update_state();

		physics.update(frame_timer.delta_seconds());

		for (auto &cube_prop : cubes) {
			cube_prop->update();
		}

		debugger.update(camera);

		scene.update_transforms();
		scene.update_buffers(camera);

		auto start = std::chrono::steady_clock::now();

		if (!g_freeze_frustum) {
			scene.cull_frustum();
		}

		auto end = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsed_seconds = end-start;
		g_elapsed = elapsed_seconds.count();

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, video_settings.canvas.x, video_settings.canvas.y);

		const glm::mat4 m = glm::rotate(glm::mat4(1.0f), 0.001f*SDL_GetTicks(), glm::vec3(1.0f, 1.0f, 1.0f));

		shader.use();
		shader.uniform_mat4("MODEL", glm::mat4(1.f));

		shader.uniform_bool("WIRED_MODE", false);
		shader.uniform_bool("INDIRECT_DRAW", true);
		//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		scene.display();
		//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		shader.uniform_bool("WIRED_MODE", true);
		debugger.display();

		/*
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		shader.uniform_bool("WIRED_MODE", true);
		shader.uniform_bool("INDIRECT_DRAW", false);
		sphere_model.display();
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		*/

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(window);

		frame_timer.finish();

		if (util::InputManager::exit_request()) {
			state = EngineState::EXIT;
		}
	}
	
	physics.clear_objects();
}

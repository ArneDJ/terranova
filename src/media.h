
// loads and stores models and textures in a map
class MediaManager {
public:
	static const gfx::Model* load_model(const std::string &filepath);
	static const gfx::Texture* load_texture(const std::string &filepath);
	static const ozz::animation::Skeleton* load_skeleton(const std::string &filepath);
	static const ozz::animation::Animation* load_animation(const std::string &filepath);
	static void clear();
private:
	static std::map<uint64_t, std::unique_ptr<gfx::Model>> m_models;
	static std::map<uint64_t, std::unique_ptr<gfx::Texture>> m_textures;
	static std::map<uint64_t, std::unique_ptr<ozz::animation::Skeleton>> m_skeletons;
	static std::map<uint64_t, std::unique_ptr<ozz::animation::Animation>> m_animations;
};

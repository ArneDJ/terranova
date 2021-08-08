
// loads and stores models and textures in a map
class MediaManager {
public:
	static const gfx::Model* load_model(const std::string &filepath);
private:
	static std::map<uint64_t, std::unique_ptr<gfx::Model>> m_models;
};

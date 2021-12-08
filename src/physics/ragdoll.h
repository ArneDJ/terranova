namespace fysx {

// usually a ragdoll joint targets multiple skeleton joints
struct RagdollTarget {
	uint32_t joint_target = 0;
	uint32_t transform_matrix = 0;
};

/*
enum SynovialJointType : uint8_t {
	JOINT_HINGE,
	JOINT_CONE
};
*/

// the actual ragdoll bone, the shape is a capsule
class RagdollBone {
public:
	std::unique_ptr<btCollisionShape> shape;
	std::unique_ptr<btRigidBody> body;
	std::unique_ptr<btMotionState> motion;
	btTransform origin;
	// multiply the global transform with the inverse bind matrix for vertex skinning
	glm::mat4 transform;
	glm::mat4 inverse;
public:
	RagdollBone(float radius, float height, const glm::vec3 &start, const glm::quat &rotation);
};

class Ragdoll {
public:
	std::vector<std::unique_ptr<RagdollBone>> bones;
	std::vector<std::unique_ptr<btTypedConstraint>> joints;
	glm::vec3 position;
	glm::quat rotation;
public:
	void update();
	void clear();
public:
	void add_bone(float radius, float height, const glm::vec3 &origin, const glm::quat &rotation);
	void add_hinge_joint(uint32_t parent_bone, uint32_t child_bone, const glm::vec2 &limit);
};

};

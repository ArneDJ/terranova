
class BuildingEntity {
public:
	std::unique_ptr<geom::Transform> transform;
	std::unique_ptr<btMotionState> motionstate;
	std::unique_ptr<btRigidBody> body;
public:
	BuildingEntity(const glm::vec3 &pos, const glm::quat &rot, btCollisionShape *shape)
	{
		transform = std::make_unique<geom::Transform>();
		transform->position = pos;
		transform->rotation = rot;

		btTransform tran;
		tran.setIdentity();
		tran.setOrigin(fysx::vec3_to_bt(pos));
		tran.setRotation(fysx::quat_to_bt(rot));

		btVector3 inertia(0, 0, 0);
		motionstate = std::make_unique<btDefaultMotionState>(tran);
		btRigidBody::btRigidBodyConstructionInfo rbInfo(0.f, motionstate.get(), shape, inertia);
		body = std::make_unique<btRigidBody>(rbInfo);
	}
};

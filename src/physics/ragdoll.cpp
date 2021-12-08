#include <iostream>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "physical.h"
#include "ragdoll.h"

namespace fysx {

static glm::mat4 bullet_to_mat4(const btTransform &t)
{
	const btMatrix3x3 &basis = t.getBasis();

	glm::mat4 m(1.f);

	// rotation
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			m[c][r] = basis[r][c];
		}
	}
	// translation
	btVector3 origin = t.getOrigin();
	m[3][0] = origin.getX();
	m[3][1] = origin.getY();
	m[3][2] = origin.getZ();
	// unit scale
	m[0][3] = 0.0f;
	m[1][3] = 0.0f;
	m[2][3] = 0.0f;
	m[3][3] = 1.0f;

	return m;
}

RagdollBone::RagdollBone(float radius, float height, const glm::vec3 &start, const glm::quat &rotation)
{
	shape = std::make_unique<btCapsuleShape>(btScalar(radius), btScalar(height));

	origin.setIdentity();
	origin.setOrigin(vec3_to_bt(start));
	origin.setRotation(quat_to_bt(rotation));
	//origin.getBasis().setEulerZYX(rotation.z, rotation.y, rotation.x);

	inverse = glm::inverse(bullet_to_mat4(origin));

	// create rigid body
	btScalar mass = 1.f;
	btVector3 intertia(0,0,0);
	if (mass != 0.f) { shape->calculateLocalInertia(mass, intertia); }

	motion = std::make_unique<btDefaultMotionState>(origin);
	btRigidBody::btRigidBodyConstructionInfo info(mass, motion.get(), shape.get(), intertia);
	info.m_additionalDamping = true;
	body = std::make_unique<btRigidBody>(info);

	// Setup some damping on the bodies
	body->setDamping(0.05f, 0.85f);
	body->setDeactivationTime(0.8f);
	body->setSleepingThresholds(1.6f, 2.5f);
	// prevent terrain clipping
	body->setContactProcessingThreshold(0.25f);
	body->setCcdMotionThreshold(0.0001f);
	body->setCcdSweptSphereRadius(0.05f);
}
	
// clears the bones and joints
void Ragdoll::clear()
{
	bones.clear();
	joints.clear();
}
	
void Ragdoll::add_bone(float radius, float height, const glm::vec3 &origin, const glm::quat &rotation)
{
	auto bone = std::make_unique<RagdollBone>(radius, height, origin, rotation);
	bones.push_back(std::move(bone));
}
	
void Ragdoll::add_hinge_joint(uint32_t parent_bone, uint32_t child_bone, const glm::vec2 &limit)
{
	btTransform localA, localB;
	localA.setIdentity();
	localB.setIdentity();
	
	if (parent_bone < bones.size() && child_bone < bones.size()) {
		auto hinge = std::make_unique<btHingeConstraint>(*bones[parent_bone]->body.get(), *bones[child_bone]->body.get(), localA, localB);
		hinge->setLimit(btScalar(limit.x), btScalar(limit.y));
		joints.push_back(std::move(hinge));
	}
}
	
void Ragdoll::update()
{
	if (bones.size()) {
		btRigidBody *root = bones[0]->body.get();
		position = body_position(root);
		rotation = body_rotation(root);
	}

	for (auto &bone : bones) {
		btRigidBody *body = bone->body.get();
		glm::mat4 T = glm::translate(glm::mat4(1.f), body_position(body));
		glm::mat4 R = glm::mat4(body_rotation(body));
		glm::mat4 global_joint_transform = T * R;
		bone->transform = global_joint_transform * bone->inverse;
	}
}

};

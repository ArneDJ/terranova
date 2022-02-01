#pragma once
#include "../util/navigation.h"
#include "../extern/recast/DetourCrowd.h"

struct NavTargetResult {
	bool found = false;
	glm::vec3 position = {};
	dtPolyRef ref;
};

class CrowdController {
public:
	CrowdController(dtNavMesh *navmesh);
	~CrowdController();
public:
	int add_agent(const glm::vec3 &start, const dtNavMeshQuery *navquery);
public:
	void teleport_agent(int index, const glm::vec3 &position);
	void retarget_agent(int index, const glm::vec3 &nearest, dtPolyRef poly);
	void retarget_agent(int index, const glm::vec3 &target, const dtNavMeshQuery *navquery);
	void set_agent_speed(int index, float speed);
public:
	void update(float delta);
public:
	const dtCrowdAgent* agent_at(int index) const;
	glm::vec3 agent_position(int index) const;
	glm::vec3 agent_velocity(int index) const;
	NavTargetResult agent_target(int index) const;
private:
	dtCrowd *dtcrowd;
};

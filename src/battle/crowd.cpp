#include <math.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <cstdio>
#include <vector>
#include <list>
#include <cstring>
#include <thread>
#include <mutex>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "crowd.h"

static const float BOX_EXTENTS[3] = { 512.f, 512.f, 512.f }; // size of box around start/end points to look for nav polygons
static const int MAX_AGENTS = 1024;
static const float MAX_AGENT_RADIUS = 1024.f;

CrowdController::CrowdController(dtNavMesh *navmesh)
{
	dtcrowd = dtAllocCrowd();
	dtcrowd->init(MAX_AGENTS, MAX_AGENT_RADIUS, navmesh);

	// Setup local avoidance params to different qualities.
	dtObstacleAvoidanceParams params;
	// Use mostly default settings, copy from dtCrowd.
	memcpy(&params, dtcrowd->getObstacleAvoidanceParams(0), sizeof(dtObstacleAvoidanceParams));

	// Low (11)
	params.velBias = 0.5f;
	params.adaptiveDivs = 5;
	params.adaptiveRings = 2;
	params.adaptiveDepth = 1;
	dtcrowd->setObstacleAvoidanceParams(0, &params);

	// Medium (22)
	params.velBias = 0.5f;
	params.adaptiveDivs = 5;
	params.adaptiveRings = 2;
	params.adaptiveDepth = 2;
	dtcrowd->setObstacleAvoidanceParams(1, &params);

	// Good (45)
	params.velBias = 0.5f;
	params.adaptiveDivs = 7;
	params.adaptiveRings = 2;
	params.adaptiveDepth = 3;
	dtcrowd->setObstacleAvoidanceParams(2, &params);

	// High (66)
	params.velBias = 0.5f;
	params.adaptiveDivs = 7;
	params.adaptiveRings = 3;
	params.adaptiveDepth = 3;

	dtcrowd->setObstacleAvoidanceParams(3, &params);
}

CrowdController::~CrowdController()
{
	dtFreeCrowd(dtcrowd);
}
	
// returns the index of created agent
// returns -1 if no agent is created
int CrowdController::add_agent(const glm::vec3 &start, const dtNavMeshQuery *navquery)
{
	// find the start polygon
	dtQueryFilter filter;
	filter.setIncludeFlags(0xFFFF);
	filter.setExcludeFlags(0);
	filter.setAreaCost(util::POLY_AREA_GROUND, 1.f);
	dtPolyRef start_poly;
	float nearest_start[3];
	dtStatus status = navquery->findNearestPoly(glm::value_ptr(start), BOX_EXTENTS, &filter, &start_poly, nearest_start);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) {
		printf("couldn't find start polygon\n");
		return -1;
	}

	// add the agent
	dtCrowdAgentParams ap;
	memset(&ap, 0, sizeof(ap));

	ap.radius = 0.5f;
	ap.height = 2.f;
	ap.maxAcceleration = 8.0f;
	ap.maxSpeed = 8.f;
	ap.collisionQueryRange = ap.radius * 12.0f;
	ap.pathOptimizationRange = ap.radius * 30.0f;
	ap.updateFlags = 0;
	//if (m_toolParams.m_anticipateTurns)
	ap.updateFlags |= DT_CROWD_ANTICIPATE_TURNS;
	//if (m_toolParams.m_optimizeVis)
	ap.updateFlags |= DT_CROWD_OPTIMIZE_VIS;
	//if (m_toolParams.m_optimizeTopo)
	ap.updateFlags |= DT_CROWD_OPTIMIZE_TOPO;
	//if (m_toolParams.m_obstacleAvoidance)
	ap.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
	//if (m_toolParams.m_separation)
	ap.updateFlags |= DT_CROWD_SEPARATION;
	ap.obstacleAvoidanceType = 3;
	ap.separationWeight = 2.f;

	return dtcrowd->addAgent(nearest_start, &ap);
}
	
void CrowdController::update(float delta)
{
	dtcrowd->update(delta, nullptr);
}
	
const dtCrowdAgent* CrowdController::agent_at(int index) const
{
	return dtcrowd->getAgent(index);
}
	
glm::vec3 CrowdController::agent_position(int index) const
{
	glm::vec3 position = { 0.f, 0.f, 0.f };

	const dtCrowdAgent *agent = dtcrowd->getAgent(index);

	const float *p = agent->npos;
	position.x = p[0];
	position.y = p[1];
	position.z = p[2];

	return position;
}

glm::vec3 CrowdController::agent_velocity(int index) const
{
	glm::vec3 velocity = { 0.f, 0.f, 0.f };

	const dtCrowdAgent *agent = dtcrowd->getAgent(index);
	if (!agent->active) { return velocity; }

	const float *v = agent->vel;
	velocity.x = v[0];
	velocity.y = v[1];
	velocity.z = v[2];

	return velocity;
}
	
NavTargetResult CrowdController::agent_target(int index) const
{
	const dtCrowdAgent *agent = dtcrowd->getAgent(index);
	NavTargetResult result = {};
	if (agent) {
		result.position.x = agent->targetPos[0];
		result.position.y = agent->targetPos[1];
		result.position.z = agent->targetPos[2];
		result.ref = agent->targetRef;
		result.found = true;
	} else {
		result.found = false;
	}

	return result;
}
	
void CrowdController::teleport_agent(int index, const glm::vec3 &position)
{
	dtCrowdAgent *agent = dtcrowd->getEditableAgent(index);
	if (agent) {
		// cancel the agent's pathfinding
		dtcrowd->resetMoveTarget(index);
		// now teleport the agent
		agent->npos[0] = position.x;
		agent->npos[1] = position.y;
		agent->npos[2] = position.z;
	}
}
	
void CrowdController::retarget_agent(int index, const glm::vec3 &nearest, dtPolyRef poly)
{
	dtcrowd->requestMoveTarget(index, poly, glm::value_ptr(nearest));
}
	
void CrowdController::retarget_agent(int index, const glm::vec3 &target, const dtNavMeshQuery *navquery)
{
	dtQueryFilter filter;
	filter.setIncludeFlags(0xFFFF);
	filter.setExcludeFlags(0);
	filter.setAreaCost(util::POLY_AREA_GROUND, 1.f);

	dtPolyRef end_poly;
	float nearest_end[3];
	dtStatus status = navquery->findNearestPoly(glm::value_ptr(target), BOX_EXTENTS, &filter, &end_poly, nearest_end);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK)) {
		printf("couldn't find end polygon\n");
		return;
	}
	
	dtcrowd->requestMoveTarget(index, end_poly, nearest_end);
}
	
void CrowdController::set_agent_speed(int index, float speed)
{
	dtCrowdAgent *agent = dtcrowd->getEditableAgent(index);
	agent->params.maxSpeed = speed;
}

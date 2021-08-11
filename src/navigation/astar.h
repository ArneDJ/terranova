#pragma once
#include "../extern/stlastar/stlastar.h"

namespace nav {

class MapSearchNode {
public:
	uint32_t index;
	glm::vec2 position;
	std::vector<MapSearchNode*> neighbors;
	
	float GoalDistanceEstimate(MapSearchNode *nodeGoal);
	bool IsGoal(MapSearchNode *nodeGoal);
	bool GetSuccessors(AStarSearch<MapSearchNode> *astarsearch, MapSearchNode *parent_node);
	float GetCost(MapSearchNode *successor);
	bool IsSameState(MapSearchNode *rhs);
	//void PrintNodeInfo();
};

};

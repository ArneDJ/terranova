#include <vector>
#include <set>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>

#include "astar.h"

namespace nav {

bool MapSearchNode::IsSameState(MapSearchNode *rhs)
{
	return (index == rhs->index);
}

// Here's the heuristic function that estimates the distance from a Node
// to the Goal.
float MapSearchNode::GoalDistanceEstimate(MapSearchNode *nodeGoal)
{
	return glm::distance(position, nodeGoal->position);
}

bool MapSearchNode::IsGoal(MapSearchNode *nodeGoal)
{
	return (index == nodeGoal->index);
};

bool MapSearchNode::GetSuccessors(AStarSearch<MapSearchNode> *astarsearch, MapSearchNode *parent_node)
{
	//printf("this node\n");
	//PrintNodeInfo();
	//printf("neighbor nodes %d\n", neighbors.size());
	for (const auto &neighbor : neighbors) {
		//MapSearchNode node;
		//node.index = neighbor->index;
		//node.position = neighbor->position;
		//node.PrintNodeInfo();
		astarsearch->AddSuccessor(neighbor);
	}
	//printf("----------\n");

	return true;
}

// given this node, what does it cost to move to successor. In the case
// of our map the answer is the map terrain value at this node since that is
// conceptually where we're moving
float MapSearchNode::GetCost(MapSearchNode *successor)
{
	return 1.f;
}

/*
void MapSearchNode::PrintNodeInfo()
{
	printf("node index, %d\n", index);
	printf("node position, %f, %f\n", position.x, position.y);
}
*/

};

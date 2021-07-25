//this file is autogenerated using stringify.bat (premake --stringify) in the build folder of this project
static const char* parallelLinearBvhCL =
	"/*\n"
	"This software is provided 'as-is', without any express or implied warranty.\n"
	"In no event will the authors be held liable for any damages arising from the use of this software.\n"
	"Permission is granted to anyone to use this software for any purpose,\n"
	"including commercial applications, and to alter it and redistribute it freely,\n"
	"subject to the following restrictions:\n"
	"1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.\n"
	"2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.\n"
	"3. This notice may not be removed or altered from any source distribution.\n"
	"*/\n"
	"//Initial Author Jackson Lee, 2014\n"
	"typedef float b3Scalar;\n"
	"typedef float4 b3Vector3;\n"
	"#define b3Max max\n"
	"#define b3Min min\n"
	"#define b3Sqrt sqrt\n"
	"typedef struct\n"
	"{\n"
	"	unsigned int m_key;\n"
	"	unsigned int m_value;\n"
	"} SortDataCL;\n"
	"typedef struct \n"
	"{\n"
	"	union\n"
	"	{\n"
	"		float4	m_min;\n"
	"		float   m_minElems[4];\n"
	"		int			m_minIndices[4];\n"
	"	};\n"
	"	union\n"
	"	{\n"
	"		float4	m_max;\n"
	"		float   m_maxElems[4];\n"
	"		int			m_maxIndices[4];\n"
	"	};\n"
	"} b3AabbCL;\n"
	"unsigned int interleaveBits(unsigned int x)\n"
	"{\n"
	"	//........ ........ ......12 3456789A	//x\n"
	"	//....1..2 ..3..4.. 5..6..7. .8..9..A	//x after interleaving bits\n"
	"	\n"
	"	//......12 3456789A ......12 3456789A	//x ^ (x << 16)\n"
	"	//11111111 ........ ........ 11111111	//0x FF 00 00 FF\n"
	"	//......12 ........ ........ 3456789A	//x = (x ^ (x << 16)) & 0xFF0000FF;\n"
	"	\n"
	"	//......12 ........ 3456789A 3456789A	//x ^ (x <<  8)\n"
	"	//......11 ........ 1111.... ....1111	//0x 03 00 F0 0F\n"
	"	//......12 ........ 3456.... ....789A	//x = (x ^ (x <<  8)) & 0x0300F00F;\n"
	"	\n"
	"	//..12..12 ....3456 3456.... 789A789A	//x ^ (x <<  4)\n"
	"	//......11 ....11.. ..11.... 11....11	//0x 03 0C 30 C3\n"
	"	//......12 ....34.. ..56.... 78....9A	//x = (x ^ (x <<  4)) & 0x030C30C3;\n"
	"	\n"
	"	//....1212 ..3434.. 5656..78 78..9A9A	//x ^ (x <<  2)\n"
	"	//....1..1 ..1..1.. 1..1..1. .1..1..1	//0x 09 24 92 49\n"
	"	//....1..2 ..3..4.. 5..6..7. .8..9..A	//x = (x ^ (x <<  2)) & 0x09249249;\n"
	"	\n"
	"	//........ ........ ......11 11111111	//0x000003FF\n"
	"	x &= 0x000003FF;		//Clear all bits above bit 10\n"
	"	\n"
	"	x = (x ^ (x << 16)) & 0xFF0000FF;\n"
	"	x = (x ^ (x <<  8)) & 0x0300F00F;\n"
	"	x = (x ^ (x <<  4)) & 0x030C30C3;\n"
	"	x = (x ^ (x <<  2)) & 0x09249249;\n"
	"	\n"
	"	return x;\n"
	"}\n"
	"unsigned int getMortonCode(unsigned int x, unsigned int y, unsigned int z)\n"
	"{\n"
	"	return interleaveBits(x) << 0 | interleaveBits(y) << 1 | interleaveBits(z) << 2;\n"
	"}\n"
	"__kernel void separateAabbs(__global b3AabbCL* unseparatedAabbs, __global int* aabbIndices, __global b3AabbCL* out_aabbs, int numAabbsToSeparate)\n"
	"{\n"
	"	int separatedAabbIndex = get_global_id(0);\n"
	"	if(separatedAabbIndex >= numAabbsToSeparate) return;\n"
	"	int unseparatedAabbIndex = aabbIndices[separatedAabbIndex];\n"
	"	out_aabbs[separatedAabbIndex] = unseparatedAabbs[unseparatedAabbIndex];\n"
	"}\n"
	"//Should replace with an optimized parallel reduction\n"
	"__kernel void findAllNodesMergedAabb(__global b3AabbCL* out_mergedAabb, int numAabbsNeedingMerge)\n"
	"{\n"
	"	//Each time this kernel is added to the command queue, \n"
	"	//the number of AABBs needing to be merged is halved\n"
	"	//\n"
	"	//Example with 159 AABBs:\n"
	"	//	numRemainingAabbs == 159 / 2 + 159 % 2 == 80\n"
	"	//	numMergedAabbs == 159 - 80 == 79\n"
	"	//So, indices [0, 78] are merged with [0 + 80, 78 + 80]\n"
	"	\n"
	"	int numRemainingAabbs = numAabbsNeedingMerge / 2 + numAabbsNeedingMerge % 2;\n"
	"	int numMergedAabbs = numAabbsNeedingMerge - numRemainingAabbs;\n"
	"	\n"
	"	int aabbIndex = get_global_id(0);\n"
	"	if(aabbIndex >= numMergedAabbs) return;\n"
	"	\n"
	"	int otherAabbIndex = aabbIndex + numRemainingAabbs;\n"
	"	\n"
	"	b3AabbCL aabb = out_mergedAabb[aabbIndex];\n"
	"	b3AabbCL otherAabb = out_mergedAabb[otherAabbIndex];\n"
	"		\n"
	"	b3AabbCL mergedAabb;\n"
	"	mergedAabb.m_min = b3Min(aabb.m_min, otherAabb.m_min);\n"
	"	mergedAabb.m_max = b3Max(aabb.m_max, otherAabb.m_max);\n"
	"	out_mergedAabb[aabbIndex] = mergedAabb;\n"
	"}\n"
	"__kernel void assignMortonCodesAndAabbIndicies(__global b3AabbCL* worldSpaceAabbs, __global b3AabbCL* mergedAabbOfAllNodes, \n"
	"												__global SortDataCL* out_mortonCodesAndAabbIndices, int numAabbs)\n"
	"{\n"
	"	int leafNodeIndex = get_global_id(0);	//Leaf node index == AABB index\n"
	"	if(leafNodeIndex >= numAabbs) return;\n"
	"	\n"
	"	b3AabbCL mergedAabb = mergedAabbOfAllNodes[0];\n"
	"	b3Vector3 gridCenter = (mergedAabb.m_min + mergedAabb.m_max) * 0.5f;\n"
	"	b3Vector3 gridCellSize = (mergedAabb.m_max - mergedAabb.m_min) / (float)1024;\n"
	"	\n"
	"	b3AabbCL aabb = worldSpaceAabbs[leafNodeIndex];\n"
	"	b3Vector3 aabbCenter = (aabb.m_min + aabb.m_max) * 0.5f;\n"
	"	b3Vector3 aabbCenterRelativeToGrid = aabbCenter - gridCenter;\n"
	"	\n"
	"	//Quantize into integer coordinates\n"
	"	//floor() is needed to prevent the center cell, at (0,0,0) from being twice the size\n"
	"	b3Vector3 gridPosition = aabbCenterRelativeToGrid / gridCellSize;\n"
	"	\n"
	"	int4 discretePosition;\n"
	"	discretePosition.x = (int)( (gridPosition.x >= 0.0f) ? gridPosition.x : floor(gridPosition.x) );\n"
	"	discretePosition.y = (int)( (gridPosition.y >= 0.0f) ? gridPosition.y : floor(gridPosition.y) );\n"
	"	discretePosition.z = (int)( (gridPosition.z >= 0.0f) ? gridPosition.z : floor(gridPosition.z) );\n"
	"	\n"
	"	//Clamp coordinates into [-512, 511], then convert range from [-512, 511] to [0, 1023]\n"
	"	discretePosition = b3Max( -512, b3Min(discretePosition, 511) );\n"
	"	discretePosition += 512;\n"
	"	\n"
	"	//Interleave bits(assign a morton code, also known as a z-curve)\n"
	"	unsigned int mortonCode = getMortonCode(discretePosition.x, discretePosition.y, discretePosition.z);\n"
	"	\n"
	"	//\n"
	"	SortDataCL mortonCodeIndexPair;\n"
	"	mortonCodeIndexPair.m_key = mortonCode;\n"
	"	mortonCodeIndexPair.m_value = leafNodeIndex;\n"
	"	\n"
	"	out_mortonCodesAndAabbIndices[leafNodeIndex] = mortonCodeIndexPair;\n"
	"}\n"
	"#define B3_PLVBH_TRAVERSE_MAX_STACK_SIZE 128\n"
	"//The most significant bit(0x80000000) of a int32 is used to distinguish between leaf and internal nodes.\n"
	"//If it is set, then the index is for an internal node; otherwise, it is a leaf node. \n"
	"//In both cases, the bit should be cleared to access the actual node index.\n"
	"int isLeafNode(int index) { return (index >> 31 == 0); }\n"
	"int getIndexWithInternalNodeMarkerRemoved(int index) { return index & (~0x80000000); }\n"
	"int getIndexWithInternalNodeMarkerSet(int isLeaf, int index) { return (isLeaf) ? index : (index | 0x80000000); }\n"
	"//From sap.cl\n"
	"#define NEW_PAIR_MARKER -1\n"
	"bool TestAabbAgainstAabb2(const b3AabbCL* aabb1, const b3AabbCL* aabb2)\n"
	"{\n"
	"	bool overlap = true;\n"
	"	overlap = (aabb1->m_min.x > aabb2->m_max.x || aabb1->m_max.x < aabb2->m_min.x) ? false : overlap;\n"
	"	overlap = (aabb1->m_min.z > aabb2->m_max.z || aabb1->m_max.z < aabb2->m_min.z) ? false : overlap;\n"
	"	overlap = (aabb1->m_min.y > aabb2->m_max.y || aabb1->m_max.y < aabb2->m_min.y) ? false : overlap;\n"
	"	return overlap;\n"
	"}\n"
	"//From sap.cl\n"
	"__kernel void plbvhCalculateOverlappingPairs(__global b3AabbCL* rigidAabbs, \n"
	"											__global int* rootNodeIndex, \n"
	"											__global int2* internalNodeChildIndices, \n"
	"											__global b3AabbCL* internalNodeAabbs,\n"
	"											__global int2* internalNodeLeafIndexRanges,\n"
	"											\n"
	"											__global SortDataCL* mortonCodesAndAabbIndices,\n"
	"											__global int* out_numPairs, __global int4* out_overlappingPairs, \n"
	"											int maxPairs, int numQueryAabbs)\n"
	"{\n"
	"	//Using get_group_id()/get_local_id() is Faster than get_global_id(0) since\n"
	"	//mortonCodesAndAabbIndices[] contains rigid body indices sorted along the z-curve (more spatially coherent)\n"
	"	int queryBvhNodeIndex = get_group_id(0) * get_local_size(0) + get_local_id(0);\n"
	"	if(queryBvhNodeIndex >= numQueryAabbs) return;\n"
	"	\n"
	"	int queryRigidIndex = mortonCodesAndAabbIndices[queryBvhNodeIndex].m_value;\n"
	"	b3AabbCL queryAabb = rigidAabbs[queryRigidIndex];\n"
	"	\n"
	"	int stack[B3_PLVBH_TRAVERSE_MAX_STACK_SIZE];\n"
	"	\n"
	"	int stackSize = 1;\n"
	"	stack[0] = *rootNodeIndex;\n"
	"	\n"
	"	while(stackSize)\n"
	"	{\n"
	"		int internalOrLeafNodeIndex = stack[ stackSize - 1 ];\n"
	"		--stackSize;\n"
	"		\n"
	"		int isLeaf = isLeafNode(internalOrLeafNodeIndex);	//Internal node if false\n"
	"		int bvhNodeIndex = getIndexWithInternalNodeMarkerRemoved(internalOrLeafNodeIndex);\n"
	"		\n"
	"		//Optimization - if the BVH is structured as a binary radix tree, then\n"
	"		//each internal node corresponds to a contiguous range of leaf nodes(internalNodeLeafIndexRanges[]).\n"
	"		//This can be used to avoid testing each AABB-AABB pair twice, including preventing each node from colliding with itself.\n"
	"		{\n"
	"			int highestLeafIndex = (isLeaf) ? bvhNodeIndex : internalNodeLeafIndexRanges[bvhNodeIndex].y;\n"
	"			if(highestLeafIndex <= queryBvhNodeIndex) continue;\n"
	"		}\n"
	"		\n"
	"		//bvhRigidIndex is not used if internal node\n"
	"		int bvhRigidIndex = (isLeaf) ? mortonCodesAndAabbIndices[bvhNodeIndex].m_value : -1;\n"
	"	\n"
	"		b3AabbCL bvhNodeAabb = (isLeaf) ? rigidAabbs[bvhRigidIndex] : internalNodeAabbs[bvhNodeIndex];\n"
	"		if( TestAabbAgainstAabb2(&queryAabb, &bvhNodeAabb) )\n"
	"		{\n"
	"			if(isLeaf)\n"
	"			{\n"
	"				int4 pair;\n"
	"				pair.x = rigidAabbs[queryRigidIndex].m_minIndices[3];\n"
	"				pair.y = rigidAabbs[bvhRigidIndex].m_minIndices[3];\n"
	"				pair.z = NEW_PAIR_MARKER;\n"
	"				pair.w = NEW_PAIR_MARKER;\n"
	"				\n"
	"				int pairIndex = atomic_inc(out_numPairs);\n"
	"				if(pairIndex < maxPairs) out_overlappingPairs[pairIndex] = pair;\n"
	"			}\n"
	"			\n"
	"			if(!isLeaf)	//Internal node\n"
	"			{\n"
	"				if(stackSize + 2 > B3_PLVBH_TRAVERSE_MAX_STACK_SIZE)\n"
	"				{\n"
	"					//Error\n"
	"				}\n"
	"				else\n"
	"				{\n"
	"					stack[ stackSize++ ] = internalNodeChildIndices[bvhNodeIndex].x;\n"
	"					stack[ stackSize++ ] = internalNodeChildIndices[bvhNodeIndex].y;\n"
	"				}\n"
	"			}\n"
	"		}\n"
	"		\n"
	"	}\n"
	"}\n"
	"//From rayCastKernels.cl\n"
	"typedef struct\n"
	"{\n"
	"	float4 m_from;\n"
	"	float4 m_to;\n"
	"} b3RayInfo;\n"
	"//From rayCastKernels.cl\n"
	"b3Vector3 b3Vector3_normalize(b3Vector3 v)\n"
	"{\n"
	"	b3Vector3 normal = (b3Vector3){v.x, v.y, v.z, 0.f};\n"
	"	return normalize(normal);	//OpenCL normalize == vector4 normalize\n"
	"}\n"
	"b3Scalar b3Vector3_length2(b3Vector3 v) { return v.x*v.x + v.y*v.y + v.z*v.z; }\n"
	"b3Scalar b3Vector3_dot(b3Vector3 a, b3Vector3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }\n"
	"int rayIntersectsAabb(b3Vector3 rayOrigin, b3Scalar rayLength, b3Vector3 rayNormalizedDirection, b3AabbCL aabb)\n"
	"{\n"
	"	//AABB is considered as 3 pairs of 2 planes( {x_min, x_max}, {y_min, y_max}, {z_min, z_max} ).\n"
	"	//t_min is the point of intersection with the closer plane, t_max is the point of intersection with the farther plane.\n"
	"	//\n"
	"	//if (rayNormalizedDirection.x < 0.0f), then max.x will be the near plane \n"
	"	//and min.x will be the far plane; otherwise, it is reversed.\n"
	"	//\n"
	"	//In order for there to be a collision, the t_min and t_max of each pair must overlap.\n"
	"	//This can be tested for by selecting the highest t_min and lowest t_max and comparing them.\n"
	"	\n"
	"	int4 isNegative = isless( rayNormalizedDirection, ((b3Vector3){0.0f, 0.0f, 0.0f, 0.0f}) );	//isless(x,y) returns (x < y)\n"
	"	\n"
	"	//When using vector types, the select() function checks the most signficant bit, \n"
	"	//but isless() sets the least significant bit.\n"
	"	isNegative <<= 31;\n"
	"	//select(b, a, condition) == condition ? a : b\n"
	"	//When using select() with vector types, (condition[i]) is true if its most significant bit is 1\n"
	"	b3Vector3 t_min = ( select(aabb.m_min, aabb.m_max, isNegative) - rayOrigin ) / rayNormalizedDirection;\n"
	"	b3Vector3 t_max = ( select(aabb.m_max, aabb.m_min, isNegative) - rayOrigin ) / rayNormalizedDirection;\n"
	"	\n"
	"	b3Scalar t_min_final = 0.0f;\n"
	"	b3Scalar t_max_final = rayLength;\n"
	"	\n"
	"	//Must use fmin()/fmax(); if one of the parameters is NaN, then the parameter that is not NaN is returned. \n"
	"	//Behavior of min()/max() with NaNs is undefined. (See OpenCL Specification 1.2 [6.12.2] and [6.12.4])\n"
	"	//Since the innermost fmin()/fmax() is always not NaN, this should never return NaN.\n"
	"	t_min_final = fmax( t_min.z, fmax(t_min.y, fmax(t_min.x, t_min_final)) );\n"
	"	t_max_final = fmin( t_max.z, fmin(t_max.y, fmin(t_max.x, t_max_final)) );\n"
	"	\n"
	"	return (t_min_final <= t_max_final);\n"
	"}\n"
	"__kernel void plbvhRayTraverse(__global b3AabbCL* rigidAabbs,\n"
	"								__global int* rootNodeIndex, \n"
	"								__global int2* internalNodeChildIndices, \n"
	"								__global b3AabbCL* internalNodeAabbs,\n"
	"								__global int2* internalNodeLeafIndexRanges,\n"
	"								__global SortDataCL* mortonCodesAndAabbIndices,\n"
	"								\n"
	"								__global b3RayInfo* rays,\n"
	"								\n"
	"								__global int* out_numRayRigidPairs, \n"
	"								__global int2* out_rayRigidPairs,\n"
	"								int maxRayRigidPairs, int numRays)\n"
	"{\n"
	"	int rayIndex = get_global_id(0);\n"
	"	if(rayIndex >= numRays) return;\n"
	"	\n"
	"	//\n"
	"	b3Vector3 rayFrom = rays[rayIndex].m_from;\n"
	"	b3Vector3 rayTo = rays[rayIndex].m_to;\n"
	"	b3Vector3 rayNormalizedDirection = b3Vector3_normalize(rayTo - rayFrom);\n"
	"	b3Scalar rayLength = b3Sqrt( b3Vector3_length2(rayTo - rayFrom) );\n"
	"	\n"
	"	//\n"
	"	int stack[B3_PLVBH_TRAVERSE_MAX_STACK_SIZE];\n"
	"	\n"
	"	int stackSize = 1;\n"
	"	stack[0] = *rootNodeIndex;\n"
	"	\n"
	"	while(stackSize)\n"
	"	{\n"
	"		int internalOrLeafNodeIndex = stack[ stackSize - 1 ];\n"
	"		--stackSize;\n"
	"		\n"
	"		int isLeaf = isLeafNode(internalOrLeafNodeIndex);	//Internal node if false\n"
	"		int bvhNodeIndex = getIndexWithInternalNodeMarkerRemoved(internalOrLeafNodeIndex);\n"
	"		\n"
	"		//bvhRigidIndex is not used if internal node\n"
	"		int bvhRigidIndex = (isLeaf) ? mortonCodesAndAabbIndices[bvhNodeIndex].m_value : -1;\n"
	"	\n"
	"		b3AabbCL bvhNodeAabb = (isLeaf) ? rigidAabbs[bvhRigidIndex] : internalNodeAabbs[bvhNodeIndex];\n"
	"		if( rayIntersectsAabb(rayFrom, rayLength, rayNormalizedDirection, bvhNodeAabb)  )\n"
	"		{\n"
	"			if(isLeaf)\n"
	"			{\n"
	"				int2 rayRigidPair;\n"
	"				rayRigidPair.x = rayIndex;\n"
	"				rayRigidPair.y = rigidAabbs[bvhRigidIndex].m_minIndices[3];\n"
	"				\n"
	"				int pairIndex = atomic_inc(out_numRayRigidPairs);\n"
	"				if(pairIndex < maxRayRigidPairs) out_rayRigidPairs[pairIndex] = rayRigidPair;\n"
	"			}\n"
	"			\n"
	"			if(!isLeaf)	//Internal node\n"
	"			{\n"
	"				if(stackSize + 2 > B3_PLVBH_TRAVERSE_MAX_STACK_SIZE)\n"
	"				{\n"
	"					//Error\n"
	"				}\n"
	"				else\n"
	"				{\n"
	"					stack[ stackSize++ ] = internalNodeChildIndices[bvhNodeIndex].x;\n"
	"					stack[ stackSize++ ] = internalNodeChildIndices[bvhNodeIndex].y;\n"
	"				}\n"
	"			}\n"
	"		}\n"
	"	}\n"
	"}\n"
	"__kernel void plbvhLargeAabbAabbTest(__global b3AabbCL* smallAabbs, __global b3AabbCL* largeAabbs, \n"
	"									__global int* out_numPairs, __global int4* out_overlappingPairs, \n"
	"									int maxPairs, int numLargeAabbRigids, int numSmallAabbRigids)\n"
	"{\n"
	"	int smallAabbIndex = get_global_id(0);\n"
	"	if(smallAabbIndex >= numSmallAabbRigids) return;\n"
	"	\n"
	"	b3AabbCL smallAabb = smallAabbs[smallAabbIndex];\n"
	"	for(int i = 0; i < numLargeAabbRigids; ++i)\n"
	"	{\n"
	"		b3AabbCL largeAabb = largeAabbs[i];\n"
	"		if( TestAabbAgainstAabb2(&smallAabb, &largeAabb) )\n"
	"		{\n"
	"			int4 pair;\n"
	"			pair.x = largeAabb.m_minIndices[3];\n"
	"			pair.y = smallAabb.m_minIndices[3];\n"
	"			pair.z = NEW_PAIR_MARKER;\n"
	"			pair.w = NEW_PAIR_MARKER;\n"
	"			\n"
	"			int pairIndex = atomic_inc(out_numPairs);\n"
	"			if(pairIndex < maxPairs) out_overlappingPairs[pairIndex] = pair;\n"
	"		}\n"
	"	}\n"
	"}\n"
	"__kernel void plbvhLargeAabbRayTest(__global b3AabbCL* largeRigidAabbs, __global b3RayInfo* rays,\n"
	"									__global int* out_numRayRigidPairs,  __global int2* out_rayRigidPairs,\n"
	"									int numLargeAabbRigids, int maxRayRigidPairs, int numRays)\n"
	"{\n"
	"	int rayIndex = get_global_id(0);\n"
	"	if(rayIndex >= numRays) return;\n"
	"	\n"
	"	b3Vector3 rayFrom = rays[rayIndex].m_from;\n"
	"	b3Vector3 rayTo = rays[rayIndex].m_to;\n"
	"	b3Vector3 rayNormalizedDirection = b3Vector3_normalize(rayTo - rayFrom);\n"
	"	b3Scalar rayLength = b3Sqrt( b3Vector3_length2(rayTo - rayFrom) );\n"
	"	\n"
	"	for(int i = 0; i < numLargeAabbRigids; ++i)\n"
	"	{\n"
	"		b3AabbCL rigidAabb = largeRigidAabbs[i];\n"
	"		if( rayIntersectsAabb(rayFrom, rayLength, rayNormalizedDirection, rigidAabb) )\n"
	"		{\n"
	"			int2 rayRigidPair;\n"
	"			rayRigidPair.x = rayIndex;\n"
	"			rayRigidPair.y = rigidAabb.m_minIndices[3];\n"
	"			\n"
	"			int pairIndex = atomic_inc(out_numRayRigidPairs);\n"
	"			if(pairIndex < maxRayRigidPairs) out_rayRigidPairs[pairIndex] = rayRigidPair;\n"
	"		}\n"
	"	}\n"
	"}\n"
	"//Set so that it is always greater than the actual common prefixes, and never selected as a parent node.\n"
	"//If there are no duplicates, then the highest common prefix is 32 or 64, depending on the number of bits used for the z-curve.\n"
	"//Duplicate common prefixes increase the highest common prefix at most by the number of bits used to index the leaf node.\n"
	"//Since 32 bit ints are used to index leaf nodes, the max prefix is 64(32 + 32 bit z-curve) or 96(32 + 64 bit z-curve).\n"
	"#define B3_PLBVH_INVALID_COMMON_PREFIX 128\n"
	"#define B3_PLBVH_ROOT_NODE_MARKER -1\n"
	"#define b3Int64 long\n"
	"int computeCommonPrefixLength(b3Int64 i, b3Int64 j) { return (int)clz(i ^ j); }\n"
	"b3Int64 computeCommonPrefix(b3Int64 i, b3Int64 j) \n"
	"{\n"
	"	//This function only needs to return (i & j) in order for the algorithm to work,\n"
	"	//but it may help with debugging to mask out the lower bits.\n"
	"	b3Int64 commonPrefixLength = (b3Int64)computeCommonPrefixLength(i, j);\n"
	"	b3Int64 sharedBits = i & j;\n"
	"	b3Int64 bitmask = ((b3Int64)(~0)) << (64 - commonPrefixLength);	//Set all bits after the common prefix to 0\n"
	"	\n"
	"	return sharedBits & bitmask;\n"
	"}\n"
	"//Same as computeCommonPrefixLength(), but allows for prefixes with different lengths\n"
	"int getSharedPrefixLength(b3Int64 prefixA, int prefixLengthA, b3Int64 prefixB, int prefixLengthB)\n"
	"{\n"
	"	return b3Min( computeCommonPrefixLength(prefixA, prefixB), b3Min(prefixLengthA, prefixLengthB) );\n"
	"}\n"
	"__kernel void computeAdjacentPairCommonPrefix(__global SortDataCL* mortonCodesAndAabbIndices,\n"
	"											__global b3Int64* out_commonPrefixes,\n"
	"											__global int* out_commonPrefixLengths,\n"
	"											int numInternalNodes)\n"
	"{\n"
	"	int internalNodeIndex = get_global_id(0);\n"
	"	if (internalNodeIndex >= numInternalNodes) return;\n"
	"	\n"
	"	//Here, (internalNodeIndex + 1) is never out of bounds since it is a leaf node index,\n"
	"	//and the number of internal nodes is always numLeafNodes - 1\n"
	"	int leftLeafIndex = internalNodeIndex;\n"
	"	int rightLeafIndex = internalNodeIndex + 1;\n"
	"	\n"
	"	int leftLeafMortonCode = mortonCodesAndAabbIndices[leftLeafIndex].m_key;\n"
	"	int rightLeafMortonCode = mortonCodesAndAabbIndices[rightLeafIndex].m_key;\n"
	"	\n"
	"	//Binary radix tree construction algorithm does not work if there are duplicate morton codes.\n"
	"	//Append the index of each leaf node to each morton code so that there are no duplicates.\n"
	"	//The algorithm also requires that the morton codes are sorted in ascending order; this requirement\n"
	"	//is also satisfied with this method, as (leftLeafIndex < rightLeafIndex) is always true.\n"
	"	//\n"
	"	//upsample(a, b) == ( ((b3Int64)a) << 32) | b\n"
	"	b3Int64 nonduplicateLeftMortonCode = upsample(leftLeafMortonCode, leftLeafIndex);\n"
	"	b3Int64 nonduplicateRightMortonCode = upsample(rightLeafMortonCode, rightLeafIndex);\n"
	"	\n"
	"	out_commonPrefixes[internalNodeIndex] = computeCommonPrefix(nonduplicateLeftMortonCode, nonduplicateRightMortonCode);\n"
	"	out_commonPrefixLengths[internalNodeIndex] = computeCommonPrefixLength(nonduplicateLeftMortonCode, nonduplicateRightMortonCode);\n"
	"}\n"
	"__kernel void buildBinaryRadixTreeLeafNodes(__global int* commonPrefixLengths, __global int* out_leafNodeParentNodes,\n"
	"											__global int2* out_childNodes, int numLeafNodes)\n"
	"{\n"
	"	int leafNodeIndex = get_global_id(0);\n"
	"	if (leafNodeIndex >= numLeafNodes) return;\n"
	"	\n"
	"	int numInternalNodes = numLeafNodes - 1;\n"
	"	\n"
	"	int leftSplitIndex = leafNodeIndex - 1;\n"
	"	int rightSplitIndex = leafNodeIndex;\n"
	"	\n"
	"	int leftCommonPrefix = (leftSplitIndex >= 0) ? commonPrefixLengths[leftSplitIndex] : B3_PLBVH_INVALID_COMMON_PREFIX;\n"
	"	int rightCommonPrefix = (rightSplitIndex < numInternalNodes) ? commonPrefixLengths[rightSplitIndex] : B3_PLBVH_INVALID_COMMON_PREFIX;\n"
	"	\n"
	"	//Parent node is the highest adjacent common prefix that is lower than the node's common prefix\n"
	"	//Leaf nodes are considered as having the highest common prefix\n"
	"	int isLeftHigherCommonPrefix = (leftCommonPrefix > rightCommonPrefix);\n"
	"	\n"
	"	//Handle cases for the edge nodes; the first and last node\n"
	"	//For leaf nodes, leftCommonPrefix and rightCommonPrefix should never both be B3_PLBVH_INVALID_COMMON_PREFIX\n"
	"	if(leftCommonPrefix == B3_PLBVH_INVALID_COMMON_PREFIX) isLeftHigherCommonPrefix = false;\n"
	"	if(rightCommonPrefix == B3_PLBVH_INVALID_COMMON_PREFIX) isLeftHigherCommonPrefix = true;\n"
	"	\n"
	"	int parentNodeIndex = (isLeftHigherCommonPrefix) ? leftSplitIndex : rightSplitIndex;\n"
	"	out_leafNodeParentNodes[leafNodeIndex] = parentNodeIndex;\n"
	"	\n"
	"	int isRightChild = (isLeftHigherCommonPrefix);	//If the left node is the parent, then this node is its right child and vice versa\n"
	"	\n"
	"	//out_childNodesAsInt[0] == int2.x == left child\n"
	"	//out_childNodesAsInt[1] == int2.y == right child\n"
	"	int isLeaf = 1;\n"
	"	__global int* out_childNodesAsInt = (__global int*)(&out_childNodes[parentNodeIndex]);\n"
	"	out_childNodesAsInt[isRightChild] = getIndexWithInternalNodeMarkerSet(isLeaf, leafNodeIndex);\n"
	"}\n"
	"__kernel void buildBinaryRadixTreeInternalNodes(__global b3Int64* commonPrefixes, __global int* commonPrefixLengths,\n"
	"												__global int2* out_childNodes,\n"
	"												__global int* out_internalNodeParentNodes, __global int* out_rootNodeIndex,\n"
	"												int numInternalNodes)\n"
	"{\n"
	"	int internalNodeIndex = get_group_id(0) * get_local_size(0) + get_local_id(0);\n"
	"	if(internalNodeIndex >= numInternalNodes) return;\n"
	"	\n"
	"	b3Int64 nodePrefix = commonPrefixes[internalNodeIndex];\n"
	"	int nodePrefixLength = commonPrefixLengths[internalNodeIndex];\n"
	"	\n"
	"//#define USE_LINEAR_SEARCH\n"
	"#ifdef USE_LINEAR_SEARCH\n"
	"	int leftIndex = -1;\n"
	"	int rightIndex = -1;\n"
	"	\n"
	"	//Find nearest element to left with a lower common prefix\n"
	"	for(int i = internalNodeIndex - 1; i >= 0; --i)\n"
	"	{\n"
	"		int nodeLeftSharedPrefixLength = getSharedPrefixLength(nodePrefix, nodePrefixLength, commonPrefixes[i], commonPrefixLengths[i]);\n"
	"		if(nodeLeftSharedPrefixLength < nodePrefixLength)\n"
	"		{\n"
	"			leftIndex = i;\n"
	"			break;\n"
	"		}\n"
	"	}\n"
	"	\n"
	"	//Find nearest element to right with a lower common prefix\n"
	"	for(int i = internalNodeIndex + 1; i < numInternalNodes; ++i)\n"
	"	{\n"
	"		int nodeRightSharedPrefixLength = getSharedPrefixLength(nodePrefix, nodePrefixLength, commonPrefixes[i], commonPrefixLengths[i]);\n"
	"		if(nodeRightSharedPrefixLength < nodePrefixLength)\n"
	"		{\n"
	"			rightIndex = i;\n"
	"			break;\n"
	"		}\n"
	"	}\n"
	"	\n"
	"#else //Use binary search\n"
	"	//Find nearest element to left with a lower common prefix\n"
	"	int leftIndex = -1;\n"
	"	{\n"
	"		int lower = 0;\n"
	"		int upper = internalNodeIndex - 1;\n"
	"		\n"
	"		while(lower <= upper)\n"
	"		{\n"
	"			int mid = (lower + upper) / 2;\n"
	"			b3Int64 midPrefix = commonPrefixes[mid];\n"
	"			int midPrefixLength = commonPrefixLengths[mid];\n"
	"			\n"
	"			int nodeMidSharedPrefixLength = getSharedPrefixLength(nodePrefix, nodePrefixLength, midPrefix, midPrefixLength);\n"
	"			if(nodeMidSharedPrefixLength < nodePrefixLength) \n"
	"			{\n"
	"				int right = mid + 1;\n"
	"				if(right < internalNodeIndex)\n"
	"				{\n"
	"					b3Int64 rightPrefix = commonPrefixes[right];\n"
	"					int rightPrefixLength = commonPrefixLengths[right];\n"
	"					\n"
	"					int nodeRightSharedPrefixLength = getSharedPrefixLength(nodePrefix, nodePrefixLength, rightPrefix, rightPrefixLength);\n"
	"					if(nodeRightSharedPrefixLength < nodePrefixLength) \n"
	"					{\n"
	"						lower = right;\n"
	"						leftIndex = right;\n"
	"					}\n"
	"					else \n"
	"					{\n"
	"						leftIndex = mid;\n"
	"						break;\n"
	"					}\n"
	"				}\n"
	"				else \n"
	"				{\n"
	"					leftIndex = mid;\n"
	"					break;\n"
	"				}\n"
	"			}\n"
	"			else upper = mid - 1;\n"
	"		}\n"
	"	}\n"
	"	\n"
	"	//Find nearest element to right with a lower common prefix\n"
	"	int rightIndex = -1;\n"
	"	{\n"
	"		int lower = internalNodeIndex + 1;\n"
	"		int upper = numInternalNodes - 1;\n"
	"		\n"
	"		while(lower <= upper)\n"
	"		{\n"
	"			int mid = (lower + upper) / 2;\n"
	"			b3Int64 midPrefix = commonPrefixes[mid];\n"
	"			int midPrefixLength = commonPrefixLengths[mid];\n"
	"			\n"
	"			int nodeMidSharedPrefixLength = getSharedPrefixLength(nodePrefix, nodePrefixLength, midPrefix, midPrefixLength);\n"
	"			if(nodeMidSharedPrefixLength < nodePrefixLength) \n"
	"			{\n"
	"				int left = mid - 1;\n"
	"				if(left > internalNodeIndex)\n"
	"				{\n"
	"					b3Int64 leftPrefix = commonPrefixes[left];\n"
	"					int leftPrefixLength = commonPrefixLengths[left];\n"
	"				\n"
	"					int nodeLeftSharedPrefixLength = getSharedPrefixLength(nodePrefix, nodePrefixLength, leftPrefix, leftPrefixLength);\n"
	"					if(nodeLeftSharedPrefixLength < nodePrefixLength) \n"
	"					{\n"
	"						upper = left;\n"
	"						rightIndex = left;\n"
	"					}\n"
	"					else \n"
	"					{\n"
	"						rightIndex = mid;\n"
	"						break;\n"
	"					}\n"
	"				}\n"
	"				else \n"
	"				{\n"
	"					rightIndex = mid;\n"
	"					break;\n"
	"				}\n"
	"			}\n"
	"			else lower = mid + 1;\n"
	"		}\n"
	"	}\n"
	"#endif\n"
	"	\n"
	"	//Select parent\n"
	"	{\n"
	"		int leftPrefixLength = (leftIndex != -1) ? commonPrefixLengths[leftIndex] : B3_PLBVH_INVALID_COMMON_PREFIX;\n"
	"		int rightPrefixLength =  (rightIndex != -1) ? commonPrefixLengths[rightIndex] : B3_PLBVH_INVALID_COMMON_PREFIX;\n"
	"		\n"
	"		int isLeftHigherPrefixLength = (leftPrefixLength > rightPrefixLength);\n"
	"		\n"
	"		if(leftPrefixLength == B3_PLBVH_INVALID_COMMON_PREFIX) isLeftHigherPrefixLength = false;\n"
	"		else if(rightPrefixLength == B3_PLBVH_INVALID_COMMON_PREFIX) isLeftHigherPrefixLength = true;\n"
	"		\n"
	"		int parentNodeIndex = (isLeftHigherPrefixLength) ? leftIndex : rightIndex;\n"
	"		\n"
	"		int isRootNode = (leftIndex == -1 && rightIndex == -1);\n"
	"		out_internalNodeParentNodes[internalNodeIndex] = (!isRootNode) ? parentNodeIndex : B3_PLBVH_ROOT_NODE_MARKER;\n"
	"		\n"
	"		int isLeaf = 0;\n"
	"		if(!isRootNode)\n"
	"		{\n"
	"			int isRightChild = (isLeftHigherPrefixLength);	//If the left node is the parent, then this node is its right child and vice versa\n"
	"			\n"
	"			//out_childNodesAsInt[0] == int2.x == left child\n"
	"			//out_childNodesAsInt[1] == int2.y == right child\n"
	"			__global int* out_childNodesAsInt = (__global int*)(&out_childNodes[parentNodeIndex]);\n"
	"			out_childNodesAsInt[isRightChild] = getIndexWithInternalNodeMarkerSet(isLeaf, internalNodeIndex);\n"
	"		}\n"
	"		else *out_rootNodeIndex = getIndexWithInternalNodeMarkerSet(isLeaf, internalNodeIndex);\n"
	"	}\n"
	"}\n"
	"__kernel void findDistanceFromRoot(__global int* rootNodeIndex, __global int* internalNodeParentNodes,\n"
	"									__global int* out_maxDistanceFromRoot, __global int* out_distanceFromRoot, int numInternalNodes)\n"
	"{\n"
	"	if( get_global_id(0) == 0 ) atomic_xchg(out_maxDistanceFromRoot, 0);\n"
	"	int internalNodeIndex = get_global_id(0);\n"
	"	if(internalNodeIndex >= numInternalNodes) return;\n"
	"	\n"
	"	//\n"
	"	int distanceFromRoot = 0;\n"
	"	{\n"
	"		int parentIndex = internalNodeParentNodes[internalNodeIndex];\n"
	"		while(parentIndex != B3_PLBVH_ROOT_NODE_MARKER)\n"
	"		{\n"
	"			parentIndex = internalNodeParentNodes[parentIndex];\n"
	"			++distanceFromRoot;\n"
	"		}\n"
	"	}\n"
	"	out_distanceFromRoot[internalNodeIndex] = distanceFromRoot;\n"
	"	\n"
	"	//\n"
	"	__local int localMaxDistanceFromRoot;\n"
	"	if( get_local_id(0) == 0 ) localMaxDistanceFromRoot = 0;\n"
	"	barrier(CLK_LOCAL_MEM_FENCE);\n"
	"	\n"
	"	atomic_max(&localMaxDistanceFromRoot, distanceFromRoot);\n"
	"	barrier(CLK_LOCAL_MEM_FENCE);\n"
	"	\n"
	"	if( get_local_id(0) == 0 ) atomic_max(out_maxDistanceFromRoot, localMaxDistanceFromRoot);\n"
	"}\n"
	"__kernel void buildBinaryRadixTreeAabbsRecursive(__global int* distanceFromRoot, __global SortDataCL* mortonCodesAndAabbIndices,\n"
	"												__global int2* childNodes,\n"
	"												__global b3AabbCL* leafNodeAabbs, __global b3AabbCL* internalNodeAabbs,\n"
	"												int maxDistanceFromRoot, int processedDistance, int numInternalNodes)\n"
	"{\n"
	"	int internalNodeIndex = get_global_id(0);\n"
	"	if(internalNodeIndex >= numInternalNodes) return;\n"
	"	\n"
	"	int distance = distanceFromRoot[internalNodeIndex];\n"
	"	\n"
	"	if(distance == processedDistance)\n"
	"	{\n"
	"		int leftChildIndex = childNodes[internalNodeIndex].x;\n"
	"		int rightChildIndex = childNodes[internalNodeIndex].y;\n"
	"		\n"
	"		int isLeftChildLeaf = isLeafNode(leftChildIndex);\n"
	"		int isRightChildLeaf = isLeafNode(rightChildIndex);\n"
	"		\n"
	"		leftChildIndex = getIndexWithInternalNodeMarkerRemoved(leftChildIndex);\n"
	"		rightChildIndex = getIndexWithInternalNodeMarkerRemoved(rightChildIndex);\n"
	"		\n"
	"		//leftRigidIndex/rightRigidIndex is not used if internal node\n"
	"		int leftRigidIndex = (isLeftChildLeaf) ? mortonCodesAndAabbIndices[leftChildIndex].m_value : -1;\n"
	"		int rightRigidIndex = (isRightChildLeaf) ? mortonCodesAndAabbIndices[rightChildIndex].m_value : -1;\n"
	"		\n"
	"		b3AabbCL leftChildAabb = (isLeftChildLeaf) ? leafNodeAabbs[leftRigidIndex] : internalNodeAabbs[leftChildIndex];\n"
	"		b3AabbCL rightChildAabb = (isRightChildLeaf) ? leafNodeAabbs[rightRigidIndex] : internalNodeAabbs[rightChildIndex];\n"
	"		\n"
	"		b3AabbCL mergedAabb;\n"
	"		mergedAabb.m_min = b3Min(leftChildAabb.m_min, rightChildAabb.m_min);\n"
	"		mergedAabb.m_max = b3Max(leftChildAabb.m_max, rightChildAabb.m_max);\n"
	"		internalNodeAabbs[internalNodeIndex] = mergedAabb;\n"
	"	}\n"
	"}\n"
	"__kernel void findLeafIndexRanges(__global int2* internalNodeChildNodes, __global int2* out_leafIndexRanges, int numInternalNodes)\n"
	"{\n"
	"	int internalNodeIndex = get_global_id(0);\n"
	"	if(internalNodeIndex >= numInternalNodes) return;\n"
	"	\n"
	"	int numLeafNodes = numInternalNodes + 1;\n"
	"	\n"
	"	int2 childNodes = internalNodeChildNodes[internalNodeIndex];\n"
	"	\n"
	"	int2 leafIndexRange;	//x == min leaf index, y == max leaf index\n"
	"	\n"
	"	//Find lowest leaf index covered by this internal node\n"
	"	{\n"
	"		int lowestIndex = childNodes.x;		//childNodes.x == Left child\n"
	"		while( !isLeafNode(lowestIndex) ) lowestIndex = internalNodeChildNodes[ getIndexWithInternalNodeMarkerRemoved(lowestIndex) ].x;\n"
	"		leafIndexRange.x = lowestIndex;\n"
	"	}\n"
	"	\n"
	"	//Find highest leaf index covered by this internal node\n"
	"	{\n"
	"		int highestIndex = childNodes.y;	//childNodes.y == Right child\n"
	"		while( !isLeafNode(highestIndex) ) highestIndex = internalNodeChildNodes[ getIndexWithInternalNodeMarkerRemoved(highestIndex) ].y;\n"
	"		leafIndexRange.y = highestIndex;\n"
	"	}\n"
	"	\n"
	"	//\n"
	"	out_leafIndexRanges[internalNodeIndex] = leafIndexRange;\n"
	"}\n";

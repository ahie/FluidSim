#version 430 core

layout (local_size_x = 512) in;

layout (rg32i, binding = 0) uniform iimageBuffer sorted_groupID_buffer;
layout (location = 0) uniform int i;
layout (location = 1) uniform int j;

void compare(int indexi, int indexj)
{
	ivec4 a = imageLoad(sorted_groupID_buffer, indexi);
	ivec4 b = imageLoad(sorted_groupID_buffer, indexj);
	if(b.y > a.y)
	{
		imageStore(sorted_groupID_buffer, indexi, b);
		imageStore(sorted_groupID_buffer, indexj, a);
	}
}

void main(void)
{
	float workID = 2 * gl_GlobalInvocationID.x;
	if(j == 0) // BROWN BLOCK
	{
		float blockSize = pow(2,i);
		float blockOffset = floor(workID / blockSize);
		float positionInBlock = mod(workID, blockSize) / 2;
		compare(int(blockOffset * blockSize + positionInBlock), int((blockOffset + 1) * blockSize - (positionInBlock + 1)));
	}
	else // RED BLOCK
	{
		float blockSize = pow(2,j);
		float blockOffset = floor(workID / blockSize);
		float positionInBlock = mod(workID, blockSize) / 2;
		int indexi = int(blockOffset * blockSize + positionInBlock);
		compare(indexi, indexi + int(pow(2, j-1)));
	}
}
#version 430 core

layout (local_size_x = 256) in;

layout (rg32i, binding = 0) uniform writeonly iimageBuffer groupID_buffer;
layout (rg32i, binding = 1) uniform writeonly iimageBuffer sorted_groupID_buffer;
layout (rgba32f, binding = 2) uniform readonly imageBuffer position_buffer;

layout (location = 0) uniform vec3 grid_origin;
layout (location = 1) uniform int grid_cell_count;
layout (location = 2) uniform float grid_cell_size;

void main(void)
{
	int i = int(gl_GlobalInvocationID.x);
	vec4 pos = imageLoad(position_buffer, i);
	vec3 dist = vec3(pos.x - grid_origin.x, pos.y - grid_origin.y, pos.z - grid_origin.z);
	ivec4 groupID;

	if(dist.x <= 0 || dist.y <= 0 || dist.z <= 0)
	{
		groupID = ivec4(i, -1, 0, 0);
	}
	else
	{
		int ix = int(floor(dist.x / grid_cell_size));
		int iy = int(floor(dist.y / grid_cell_size));
		int iz = int(floor(dist.z / grid_cell_size));

		if(ix >= grid_cell_count || iy >= grid_cell_count || ix >= grid_cell_count)
		{
			groupID = ivec4(i, -1, 0, 0);
		}
		else
		{
			groupID = ivec4(i, ix + iy * grid_cell_count + iz * grid_cell_count * grid_cell_count, 0, 0);
		}
	}
	imageStore(groupID_buffer, i, groupID);
	imageStore(sorted_groupID_buffer, i, groupID);
}
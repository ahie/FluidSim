#version 430 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (rg32i, binding = 0) uniform iimageBuffer sorted_groupID_buffer;
layout (rg32i, binding = 1) uniform iimage3D offset_texture;

layout (location = 0) uniform int max_particle_count;
layout (location = 1) uniform int particle_count_power;
layout (location = 2) uniform int grid_cell_count;

int getEndOffset(int cellID, int startOffset)
{
		while(startOffset + 1 < max_particle_count && imageLoad(sorted_groupID_buffer, startOffset + 1).y == cellID)
		{
			++startOffset;
		}
		return startOffset;
}

int getStartOffset(int cellID, float rangeMin, float rangeMax)
{
	float mid;
	for(int i=0; i < particle_count_power; i++)
	{
		mid = rangeMin + (rangeMax - rangeMin) / 2;
		int key = imageLoad(sorted_groupID_buffer, int(ceil(mid))).y;
		if(key < cellID)
		{
			rangeMax = floor(mid);
		}
		else if(key > cellID)
		{
			rangeMin = ceil(mid);
		}
		else
		{
			mid = ceil(mid);
			while(mid - 1 >= 0 && imageLoad(sorted_groupID_buffer, int(mid - 1)).y == cellID)
			{
				--mid;
			}
			return int(mid);
		}
	}
	return -1;
}

void main(void)
{
	if(imageLoad(offset_texture, ivec3(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, gl_GlobalInvocationID.z)) != ivec4(-1, -1, -1, -1))
	{
		int ID = int(gl_GlobalInvocationID.x + grid_cell_count * gl_GlobalInvocationID.y
						 + grid_cell_count * grid_cell_count * gl_GlobalInvocationID.z);
		int start = getStartOffset(ID, 0, max_particle_count - 1);
		imageStore(offset_texture,
			ivec3(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, gl_GlobalInvocationID.z),
				ivec4(start, getEndOffset(ID, start), -1, -1));
	}
}
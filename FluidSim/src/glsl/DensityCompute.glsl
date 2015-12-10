#version 430 core

layout (local_size_x = 128) in;

layout (rg32i, binding = 0) uniform iimageBuffer groupID_buffer;
layout (rg32i, binding = 1) uniform iimageBuffer sorted_groupID_buffer;
layout (rg32i, binding = 2) uniform iimageBuffer offset_texture;
layout (rgba32f, binding = 3) uniform imageBuffer position_read_buffer;

layout (location = 0) uniform uint grid_cell_count;
layout (location = 1) uniform float smoothing_length;
layout (location = 2) uniform float particle_mass;
layout (location = 3) uniform int max_particle_count;
layout (location = 4) uniform int particle_count_power;

layout (location = 5) uniform int buffer_offset;
int invocation_id = int(gl_GlobalInvocationID.x) + buffer_offset;

const float poly6KernelConst = 315.0 / (64.0 * 3.14159265 * pow(smoothing_length, 9.0f));
const float smoothing_length_second_power = pow(smoothing_length, 2.0f);

vec3 own_position = vec3(imageLoad(position_read_buffer, invocation_id));
float density = 0.0f;

void calcContribution(int offset)
{
	float dist = length(own_position - vec3(imageLoad(position_read_buffer, imageLoad(sorted_groupID_buffer, offset).x)));
	if (dist == 0)
	{
		density += particle_mass * poly6KernelConst * 
			pow(smoothing_length_second_power, 3.0f);
	} else if(dist < smoothing_length)
	{
		density += particle_mass * poly6KernelConst * 
			pow(smoothing_length_second_power - pow(dist,2.0f),3.0f);
	}
}

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
	float ID = float(imageLoad(groupID_buffer, invocation_id).y);
	float gccf = float(grid_cell_count);
	if(ID > -.9f)
	{
		int xi = int(mod(ID, gccf));
		int yi = int(floor(mod(ID, gccf * gccf) / gccf));
		int zi = int(floor(mod(ID, gccf * gccf * gccf)/(gccf * gccf)));

		int counter = 0;
		for(int x = -1; x < 2; x++)
		{
			for(int y = -1; y < 2; y++)
			{
				for(int z = -1; z < 2; z++)
				{
					int xj = xi + x;
					int yj = yi + y;
					int zj = zi + z;

					if( 0 <= xj && xj < grid_cell_count &&
						0 <= yj && yj < grid_cell_count &&
						0 <= zj && zj < grid_cell_count )
					{
						int CID = xj + int(grid_cell_count) * yj + int(grid_cell_count) * int(grid_cell_count) * zj;
						int startOffset = getStartOffset(CID, 0, max_particle_count - 1);
						if(startOffset != -1) // cell not empty
						{
							int endOffset = getEndOffset(CID, startOffset);
							imageStore(offset_texture, invocation_id * 27 + counter, ivec4(startOffset, endOffset, 0, 0));
							for(int i = startOffset; i <= endOffset; i++)
							{
								calcContribution(i);
							}
						}
						else
						{
							imageStore(offset_texture, invocation_id * 27 + counter, ivec4(-1));
						}
					}
					else
					{
						imageStore(offset_texture, invocation_id * 27 + counter, ivec4(-1));
					}
					counter += 1;
				}
			}
		}
		imageStore(position_read_buffer, invocation_id, vec4(own_position,density)); // set density
	}
}
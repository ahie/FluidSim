#version 430 core

layout (local_size_x = 128) in;

layout (rg32i, binding = 0) uniform iimageBuffer groupID_buffer;
layout (rg32i, binding = 1) uniform iimageBuffer sorted_groupID_buffer;
layout (rg32i, binding = 2) uniform iimageBuffer offset_texture;
layout (rgba32f, binding = 3) uniform imageBuffer position_buffer;

layout (location = 0) uniform uint grid_cell_count;
layout (location = 1) uniform float smoothing_length;
layout (location = 2) uniform float particle_mass;
layout (location = 3) uniform int max_particle_count;
layout (location = 4) uniform int particle_count_power;

int invocation_id = imageLoad(sorted_groupID_buffer, int(gl_GlobalInvocationID.x)).x;

const float poly6KernelConst = 315.0 / (64.0 * 3.14159265 * pow(smoothing_length, 9.0f));
const float smoothing_length_second_power = pow(smoothing_length, 2.0f);

vec3 own_position = vec3(imageLoad(position_buffer, invocation_id));
float density = 0.0f;

void calcContribution(int offset)
{
	float dist = length(own_position - vec3(imageLoad(position_buffer, imageLoad(sorted_groupID_buffer, offset).x)));
	if (dist == 0)
	{
		density += particle_mass * poly6KernelConst * 
			pow(smoothing_length_second_power, 3.0f);
	}
	else if(dist < smoothing_length)
	{
		density += particle_mass * poly6KernelConst * 
			pow(smoothing_length_second_power - pow(dist,2.0f),3.0f);
	}
}

void main(void)
{

	float ID = float(imageLoad(groupID_buffer, invocation_id).y);
	if(ID > -.9f)
	{
		for(int i = 0; i < 27; i++)
		{
			ivec4 offsets = imageLoad(offset_texture, invocation_id * 27 + i);
			int startOffset = offsets.r;
			if(startOffset != -1)
			{
				int endOffset = offsets.g;
				for(int j = startOffset; j <= endOffset; j++)
				{
					calcContribution(j);
				}
			}
		}
		imageStore(position_buffer, invocation_id, vec4(own_position,density));
	}
}
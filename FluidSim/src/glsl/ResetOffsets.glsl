#version 430 core

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout (rg32i, binding = 0) uniform iimage3D offset_texture;

void main(void)
{
	imageStore(offset_texture,
		ivec3(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, gl_GlobalInvocationID.z),
			ivec4(-1,-1,-1,-1));
}
#version 430 core

layout (local_size_x = 256) in;

layout (rgba32f, binding = 3) uniform imageBuffer position_buffer;
layout (rgba32f, binding = 4) uniform imageBuffer accel_buffer;
layout (rgba32f, binding = 5) uniform imageBuffer half_velocity_buffer;
layout (rgba32f, binding = 6) uniform imageBuffer velocity_buffer;

layout (location = 6) uniform float time_step;

int invocation_id = int(gl_GlobalInvocationID.x);

vec3 current_accel      = vec3(imageLoad(accel_buffer, invocation_id));
vec3 current_pos        = vec3(imageLoad(position_buffer, invocation_id));
vec3 current_half_vel   = vec3(imageLoad(half_velocity_buffer, invocation_id));
vec3 current_vel        = vec3(imageLoad(velocity_buffer, invocation_id));

float damping_factor = 0.9;

void moveParticle()
{
	vec3 new_half_vel;
	vec3 new_vel;
	vec3 new_pos;
	if(imageLoad(half_velocity_buffer, invocation_id).a < 0.f) // initialize leapfrog
	{
		new_half_vel = current_vel + 0.5f * time_step * current_accel;
		new_vel = current_vel + time_step * current_accel;
		new_pos = current_pos + time_step * new_half_vel;
	}
	else
	{
		new_half_vel = current_half_vel + time_step * current_accel;
		new_vel = new_half_vel + time_step * current_accel * 0.5f;
		new_pos = current_pos + time_step * new_half_vel;
	}

	// Simple collision detection and response
	if(new_pos.x > 10)
	{
		new_pos.x = 9.99;
		new_vel.x = -new_vel.x*damping_factor;
		new_half_vel.x = -new_half_vel.x*damping_factor;
	}
	if(new_pos.x < 0)
	{
		new_pos.x = 0.01;
		new_vel.x = -new_vel.x*damping_factor;
		new_half_vel.x = -new_half_vel.x*damping_factor;
	}
	if(new_pos.y > 10)
	{
		new_pos.y = 9.99;
		new_vel.y = -new_vel.y*damping_factor;
		new_half_vel.y = -new_half_vel.y*damping_factor;
	}
	if(new_pos.y < 0)
	{
		new_pos.y = 0.01;
		new_vel.y = -new_vel.y*damping_factor;
		new_half_vel.y = -new_half_vel.y*damping_factor;
	}
	if(new_pos.z > 10)
	{
		new_pos.z = 9.99;
		new_vel.z = -new_vel.z*damping_factor;
		new_half_vel.z = -new_half_vel.z*damping_factor;
	}
	if(new_pos.z < 0)
	{
		new_pos.z = 0.01;
		new_vel.z = -new_vel.z*damping_factor;
		new_half_vel.z = -new_half_vel.z*damping_factor;
	}

	imageStore(half_velocity_buffer, invocation_id, vec4(new_half_vel,0.f));
	imageStore(velocity_buffer, invocation_id, vec4(new_vel,0.f));
	imageStore(position_buffer, invocation_id, vec4(new_pos,imageLoad(position_buffer, invocation_id).a));
}


void main(void)
{
	moveParticle();
}
#version 430 core

layout (local_size_x = 128) in;

layout (rg32i, binding = 0) uniform iimageBuffer groupID_buffer;
layout (rg32i, binding = 1) uniform iimageBuffer sorted_groupID_buffer;
layout (rg32i, binding = 2) uniform iimageBuffer offset_texture;
layout (rgba32f, binding = 3) uniform imageBuffer position_read_buffer;
layout (rgba32f, binding = 4) uniform imageBuffer position_write_buffer;
layout (rgba32f, binding = 5) uniform imageBuffer velocity_read_buffer;
layout (rgba32f, binding = 6) uniform imageBuffer velocity_write_buffer;

layout (location = 0) uniform uint grid_cell_count;
layout (location = 1) uniform float smoothing_length;
layout (location = 2) uniform float particle_mass;
layout (location = 3) uniform float rest_density;
layout (location = 4) uniform float gas_constant;
layout (location = 5) uniform float viscosity;
layout (location = 6) uniform float time_step;
layout (location = 7) uniform int group;
layout (location = 8) uniform float surface_tension_coefficient;
layout (location = 9) uniform float surface_tension_threshold;

layout (location = 10) uniform int max_particle_count;
layout (location = 11) uniform int particle_count_power;

int GIIDX = int(gl_GlobalInvocationID.x) + group;

vec3 own_pos = vec3(imageLoad(position_read_buffer, GIIDX));
vec3 own_vel = vec3(imageLoad(velocity_read_buffer, GIIDX));
float own_density = imageLoad(position_read_buffer, GIIDX).w;
float own_pressure = gas_constant * (own_density - rest_density);

vec3 f_viscosity = vec3(0);
vec3 f_pressure = vec3(0);

vec3 stension_normal = vec3(0);
float laplace_color_field = 0.0f;

float smoothing_length_sixth_power = pow(smoothing_length,6);
float smoothing_length_ninth_power = pow(smoothing_length,9);

float gravitational_accel = 9.81;
float damping_factor = 0.1;

float laplacePoly6Kernel(float dist)
{
	return (-945 / (32 * 3.14159265 * smoothing_length_ninth_power)) *
	 pow(pow(smoothing_length,2) - pow(dist,2), 2) * (3*pow(smoothing_length,2) - 7*pow(dist,2));
}

float gradientPoly6Kernel(float dist)
{
	return (-945 / (32 * 3.14159265 * smoothing_length_ninth_power)) * pow(pow(smoothing_length,2) - pow(dist,2), 2);
}

float gradientSpikyKernel(float dist)
{
	return (45.0 / (3.14159265 * smoothing_length_sixth_power)) * pow((smoothing_length - dist),2.0f);
}

float laplaceViscosityKernel(float dist)
{
	return (45.0 / (2 * 3.14159265 * smoothing_length_sixth_power)) * (smoothing_length - dist);
}

void particleInteract(int offset)
{
	vec4 particle_pos = imageLoad(position_read_buffer, imageLoad(sorted_groupID_buffer, offset).x);
	vec3 dirvec = own_pos - vec3(particle_pos);
	float dist = length(dirvec);
	if(dist < smoothing_length && dist > 0)
	{
		float particle_density = particle_pos.w;
		float particle_pressure = gas_constant * (particle_density - rest_density);
		vec3 particle_vel = vec3(imageLoad(velocity_read_buffer, imageLoad(sorted_groupID_buffer, offset).x));

		f_pressure += particle_mass * ((particle_pressure + own_pressure) / (2 * particle_density)) * gradientSpikyKernel(dist) * normalize(dirvec);
		f_viscosity += particle_mass * ((particle_vel - own_vel) / particle_density) * laplaceViscosityKernel(dist);

		stension_normal += (particle_mass / particle_density) * gradientPoly6Kernel(dist) * dirvec;
		laplace_color_field += (particle_mass / particle_density) * laplacePoly6Kernel(dist);
	}
}

void moveParticle(vec3 accel)
{
	// TODO: leapfrog integration
	vec3 v = own_vel + time_step * accel;
	vec3 p = own_pos + time_step * v;

	// Simple collision detection and response
	if(p.x > -0.5)
	{
		p.x = -0.5;
		v.x = -v.x*damping_factor;
	}
	if(p.x < -8.5)
	{
		p.x = -8.5;
		v.x = -v.x*damping_factor;
	}
	if(p.y > -0.5)
	{
		p.y = -0.5;
		v.y = -v.y*damping_factor;
	}
	if(p.y < -8.5)
	{
		p.y = -8.5;
		v.y = -v.y*damping_factor;
	}
	if(p.z > -0.5)
	{
		p.z = -0.5;
		v.z = -v.z*damping_factor;
	}
	if(p.z < -8.5)
	{
		p.z = -8.5;
		v.z = -v.z*damping_factor;
	}

	imageStore(velocity_write_buffer, GIIDX, vec4(v,0));
	imageStore(position_write_buffer, GIIDX, vec4(p,own_density));
}

void applyForces()
{
	f_viscosity = viscosity * f_viscosity;
	float stension_normal_length = length(stension_normal);
	if(stension_normal_length > surface_tension_threshold)
	{
		vec3 f_stension = -surface_tension_coefficient * laplace_color_field * (stension_normal / stension_normal_length);
		vec3 accel = ((f_pressure + f_viscosity + f_stension)/own_density) + gravitational_accel * vec3(0, -1, 0);
		moveParticle(accel);
	} else
	{
		vec3 accel = ((f_pressure + f_viscosity)/own_density) + gravitational_accel * vec3(0, -1, 0);
		moveParticle(accel);
	}
}

void main(void)
{
	float ID = float(imageLoad(groupID_buffer, GIIDX).y);
	if(ID > -.9f)
	{
		for(int i = 0; i < 27; i++)
		{
			ivec4 offsets = imageLoad(offset_texture, GIIDX * 27 + i);
			int startOffset = offsets.r;
			if(startOffset != -1)
			{
				int endOffset = offsets.g;
				for(int j = startOffset; j <= endOffset; j++)
				{
					particleInteract(j);
				}
			}
		}
		applyForces();
	}
}
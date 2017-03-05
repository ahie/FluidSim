#version 430 core

layout (local_size_x = 128) in;

layout (rg32i, binding = 0) uniform iimageBuffer groupID_buffer;
layout (rg32i, binding = 1) uniform iimageBuffer sorted_groupID_buffer;
layout (rg32i, binding = 2) uniform iimageBuffer offset_texture;
layout (rgba32f, binding = 3) uniform imageBuffer position_buffer;
layout (rgba32f, binding = 4) uniform imageBuffer accel_buffer;
layout (rgba32f, binding = 5) uniform imageBuffer half_velocity_buffer;
layout (rgba32f, binding = 6) uniform imageBuffer velocity_buffer;

layout (location = 1) uniform float smoothing_length;
layout (location = 2) uniform float particle_mass;
layout (location = 3) uniform float rest_density;
layout (location = 4) uniform float gas_constant;
layout (location = 5) uniform float viscosity;
layout (location = 7) uniform float surface_tension_coefficient;
layout (location = 8) uniform float surface_tension_threshold;

int invocation_id = imageLoad(sorted_groupID_buffer, int(gl_GlobalInvocationID.x)).x;

vec3 own_pos = vec3(imageLoad(position_buffer, invocation_id));
vec3 own_vel = vec3(imageLoad(velocity_buffer, invocation_id));
float own_density = imageLoad(position_buffer, invocation_id).w;
float own_pressure = gas_constant * (own_density - rest_density);

vec3 f_viscosity = vec3(0);
vec3 f_pressure = vec3(0);
vec3 stension_normal = vec3(0);
float laplace_color_field = 0.0f;

const float smoothing_length_sixth_power = pow(smoothing_length,6);
const float smoothing_length_ninth_power = pow(smoothing_length,9);

float gravitational_accel = 9.81;
float damping_factor = 0.9;

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
	vec4 particle_pos = imageLoad(position_buffer, imageLoad(sorted_groupID_buffer, offset).x);
	vec3 dirvec = own_pos - vec3(particle_pos);
	float dist = length(dirvec);
	if(dist < smoothing_length && dist > 0)
	{
		float particle_density = particle_pos.w;
		float particle_pressure = gas_constant * (particle_density - rest_density);
		vec3 particle_vel = vec3(imageLoad(velocity_buffer, imageLoad(sorted_groupID_buffer, offset).x));

		f_pressure += particle_mass * ((particle_pressure + own_pressure) / (2 * particle_density)) * gradientSpikyKernel(dist) * normalize(dirvec);
		f_viscosity += particle_mass * ((particle_vel - own_vel) / particle_density) * laplaceViscosityKernel(dist);

		stension_normal += (particle_mass / particle_density) * gradientPoly6Kernel(dist) * dirvec;
		laplace_color_field += (particle_mass / particle_density) * laplacePoly6Kernel(dist);
	}
}

vec3 getAccel()
{
	f_viscosity = viscosity * f_viscosity;
	float stension_normal_length = length(stension_normal);
	if(stension_normal_length > surface_tension_threshold)
	{
		vec3 f_stension = -surface_tension_coefficient * laplace_color_field * (stension_normal / stension_normal_length);
		vec3 accel = ((f_pressure + f_viscosity + f_stension)/own_density) + gravitational_accel * vec3(0, -1, 0);
		return accel;
	} else
	{
		vec3 accel = ((f_pressure + f_viscosity)/own_density) + gravitational_accel * vec3(0, -1, 0);
		return accel;
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
					particleInteract(j);
				}
			}
		}
		imageStore(accel_buffer, invocation_id, vec4(getAccel(),own_density));
	}
}
#version 430 core

uniform mat4 MVP;

layout (location = 0) in vec4 vertexPosition;
layout (location = 1) uniform float rest_density;
out vec3 Color;

const float shift = 0.1f;

void main(void)
{
	// Visualize difference between density and rest density. 
	// Poor man's hue shift.
	float density = vertexPosition.w;
	if(density >= rest_density*(1 + shift*3))
	{
		Color = mix(vec3(1,0,0), vec3(1,1,0), clamp((rest_density*(1 + shift*4) - density)/(rest_density*shift), 0, 1));
	}
	else if(density >= rest_density*(1 + shift*2))
	{
		Color = mix(vec3(1,1,0), vec3(0,1,0), clamp((rest_density*(1 + shift*3) - density)/(rest_density*shift), 0, 1));
	}
	else if(density >= rest_density*(1 + shift*1))
	{
		Color = mix(vec3(0,1,0), vec3(0,1,1), clamp((rest_density*(1 + shift*2) - density)/(rest_density*shift), 0, 1));
	}
	else
	{
		Color = mix(vec3(0,1,1), vec3(0,0,1), clamp((rest_density*(1 + shift*1) - density)/(rest_density*shift), 0, 1));
	}

	gl_Position = MVP * vec4(vertexPosition.x, vertexPosition.y, vertexPosition.z, 1);
}
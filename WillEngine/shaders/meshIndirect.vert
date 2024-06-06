#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "indirect_input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) flat out uint outMaterialIndex;


void main()
{
	Vertex v = buffer_addresses.vertexbufferDeviceAddress.vertices[gl_VertexIndex];
	Model m = buffer_addresses.modelBufferDeviceAddress.models[gl_InstanceIndex];

	vec4 _position = vec4(v.position, 1.0);
	gl_Position =  sceneData.viewproj * m.model * _position;


	mat3 i_t_m = inverse(transpose(mat3(m.model)));
	outNormal = i_t_m *  v.normal;
	outColor = v.color.xyz;
	outUV = v.uv;//vec2(v.uv_x, v.uv_y);
	outMaterialIndex = v.material_index;
};
#version 460

#extension GL_EXT_buffer_reference : require
#extension GL_GOOGLE_include_directive : require

#include "indirect_input_structures.glsl"


//layout (location = 0) out vec3 outNormal;
//layout (location = 1) out vec3 outColor;
//layout (location = 2) out vec2 outUV;
//layout (location = 3) out uint outMaterialIndex;


void main()
{
	Vertex v = buffer_addresses.vertexbufferDeviceAddress.vertices[gl_VertexIndex];
	gl_Position =  sceneData.viewproj * v.position;
	//Model m = buffer_addresses.modelBufferDeviceAddress.models[0];

	//vec4 position = vec4(v.position, 1.0);
	//gl_Position =  sceneData.viewproj * m.model * vec4(position, 1.0f);


	//mat3 i_t_m = inverse(transpose(mat3(m.model)));
	//outNormal = mat3(i_t_m) *  normal;
	//outColor = color.xyz;
	//outUV = vec2(uv_x, uv_y);
	//outMaterialIndex = material_index;
};
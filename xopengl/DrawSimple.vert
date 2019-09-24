/*=============================================================================
	Vertexshader for DrawSimple, Line drawing.

	Revision history:
		* Created by Smirftsch
=============================================================================*/

layout (location = 0) in vec3 Coords;		// == gl_Vertex

layout(std140) uniform GlobalMatrices
{
	mat4 modelMat;
	mat4 viewMat;
	mat4 projMat;
	mat4 lightSpaceMat;
};

void main(void)
{
	mat4 modelviewprojMat = projMat * viewMat * modelMat;
	gl_Position = modelviewprojMat * vec4(Coords, 1.0);
}

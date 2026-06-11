/*=============================================================================
	DrawPostProcess.cpp: XOpenGL post-processing present shader.

	Blits the RenderFBO color texture to the default framebuffer via a
	fullscreen triangle strip generated from gl_VertexID (no vertex buffer
	needed). Also applies gamma correction here so individual draw shaders
	don't need to.

	Copyright 2014-2024 OldUnreal
=============================================================================*/

#include <glm/glm.hpp>
#include "XOpenGLDrv.h"
#include "XOpenGL.h"

/*-----------------------------------------------------------------------------
	Vertex shader
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::PostProcessProgram::BuildVertexShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << R"(
out vec2 BlitUV;
void main()
{
  float x   = (gl_VertexID & 1) != 0 ? 1.0 : -1.0;
  float y   = (gl_VertexID & 2) != 0 ? 1.0 : -1.0;
  BlitUV    = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
  gl_Position = vec4(x, y, 0.0, 1.0);
  vDrawID   = 0u;
}
)";
}

/*-----------------------------------------------------------------------------
	Fragment shader
-----------------------------------------------------------------------------*/

void UXOpenGLRenderDevice::PostProcessProgram::BuildFragmentShader(GLuint ShaderType, UXOpenGLRenderDevice* GL, FShaderWriterX& Out)
{
	Out << R"(
in vec2 BlitUV;
layout(location = 0) out vec4 FragColor;
void main()
{
  FragColor = GammaCorrect(Gamma, texture(Texture0, BlitUV));
}
)";
}

/*-----------------------------------------------------------------------------
	C++ implementation
-----------------------------------------------------------------------------*/

UXOpenGLRenderDevice::PostProcessProgram::PostProcessProgram(const TCHAR* Name, UXOpenGLRenderDevice* RenDev)
	: ShaderProgramImpl(Name, RenDev)
{
	VertexBufferSize             = 0;
	ParametersBufferSize         = 0;
	ParametersBufferBindingIndex = 0;
	NumTextureSamplers           = 1;
	DrawMode                     = GL_TRIANGLE_STRIP;
	UseSSBOParametersBuffer      = false;
	ParametersInfo               = nullptr;
	VertexShaderFunc             = &BuildVertexShader;
	GeoShaderFunc                = nullptr;
	FragmentShaderFunc           = &BuildFragmentShader;
}

void UXOpenGLRenderDevice::PostProcessProgram::CreateInputLayout() {}
void UXOpenGLRenderDevice::PostProcessProgram::Flush(bool Rotate)  {}

void UXOpenGLRenderDevice::PostProcessProgram::MapBuffers()
{
	glGenVertexArrays(1, &BlitVAO);
}

void UXOpenGLRenderDevice::PostProcessProgram::UnmapBuffers()
{
	if (BlitVAO) { glDeleteVertexArrays(1, &BlitVAO); BlitVAO = 0; }
}

void UXOpenGLRenderDevice::PostProcessProgram::ActivateShader()
{
	glDisable(GL_DEPTH_TEST);
	glBindVertexArray(BlitVAO);
	UseShader();
}

void UXOpenGLRenderDevice::PostProcessProgram::DeactivateShader()
{
	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	// Make sure we set the correct blend state again when we activate the next shader
	RenDev->CurrentBlendPolyFlags = 0;
	RenDev->SetNoTexture(DiffuseTextureIndex);
}

void UXOpenGLRenderDevice::PostProcessProgram::Draw(GLuint Texture, INT DstW, INT DstH)
{
	// Direct src-only copy — no dst contribution, without touching GL_BLEND
	// enabled state (the renderer keeps it always enabled).
	glBlendFunc(GL_ONE, GL_ZERO);
	glActiveTexture(GL_TEXTURE0 + DiffuseTextureIndex);

	// Unbind the current sampler for TMU 0 because it might use the wrong filters
	glBindSampler(DiffuseTextureIndex, 0);
	glBindTexture(GL_TEXTURE_2D, Texture);
	glViewport(0, 0, DstW, DstH);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindTexture(GL_TEXTURE_2D, 0);
}

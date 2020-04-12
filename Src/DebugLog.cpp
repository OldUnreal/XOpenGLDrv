/*=============================================================================
	DebugLog.cpp: Unreal XOpenGL debug logging OpenGL4.1 style.
	Requires at least an OpenGL 4.1 context.

	Copyright 2014-2017 Oldunreal

	Revision history:
		* Created by Smirftsch

=============================================================================*/

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "XOpenGLDrv.h"
#include "XOpenGL.h"

// Error logging
void UXOpenGLRenderDevice::DebugCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam)
{
	FString SourceMessage;
	FString TypeMessage;
	FString SeverityMessage;

	switch (source)
	{
		case GL_DEBUG_SOURCE_API:
			SourceMessage=TEXT("API");
			break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
			SourceMessage=TEXT("WINDOW_SYSTEM");
			break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER:
			SourceMessage=TEXT("SHADER_COMPILER");
			break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:
			SourceMessage=TEXT("THIRD_PARTY");
			break;
		case GL_DEBUG_SOURCE_APPLICATION:
			SourceMessage=TEXT("APPLICATION");
			break;
		case GL_DEBUG_SOURCE_OTHER:
			SourceMessage=TEXT("OTHER");
			break;
		default:
			SourceMessage=TEXT("NONE");
			break;
	}

	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR:
			TypeMessage=TEXT("ERROR");
			break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			TypeMessage=TEXT("DEPRECATED_BEHAVIOR");
			break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			TypeMessage=TEXT("UNDEFINED_BEHAVIOR");
			break;
		case GL_DEBUG_TYPE_PORTABILITY:
			TypeMessage=TEXT("PORTABILITY");
			break;
		case GL_DEBUG_TYPE_PERFORMANCE:
			TypeMessage=TEXT("PERFORMANCE");
			break;
		case GL_DEBUG_TYPE_OTHER:
			TypeMessage=TEXT("OTHER");
			break;
		default:
			TypeMessage=TEXT("NONE");
			break;
	}

	switch (severity)
	{
		case GL_DEBUG_SEVERITY_LOW:
			if (LogLevel >= 2)
				debugf(TEXT("XOpenGL DebugLog: In source %ls, type %ls, id %i, severity : LOW, message %ls "),*SourceMessage, *TypeMessage, id, appFromAnsi(message));
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			if (LogLevel >= 1)
				debugf(TEXT("XOpenGL DebugLog: In source %ls, type %ls, id %i, severity : MEDIUM, message %ls "),*SourceMessage, *TypeMessage, id, appFromAnsi(message));
			break;
		case GL_DEBUG_SEVERITY_HIGH:
			if (LogLevel >= 0)
				debugf(TEXT("XOpenGL DebugLog: In source %ls, type %ls, id %i, severity : HIGH, message %ls "),*SourceMessage, *TypeMessage, id, appFromAnsi(message));
			break;
		default:
			if (LogLevel >= 3)
				debugf(TEXT("XOpenGL DebugLog: In source %ls, type %ls, id %i, severity : NONE, message %ls "),*SourceMessage, *TypeMessage, id, appFromAnsi(message));
			break;
	}
}

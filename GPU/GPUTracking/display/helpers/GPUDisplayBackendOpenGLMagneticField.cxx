//**************************************************************************\
//* This file is property of and copyright by the ALICE Project            *\
//* ALICE Experiment at CERN, All rights reserved.                         *\
//*                                                                        *\
//* Primary Authors: Matthias Richter <Matthias.Richter@ift.uib.no>        *\
//*                  for The ALICE HLT Project.                            *\
//*                                                                        *\
//* Permission to use, copy, modify and distribute this software and its   *\
//* documentation strictly for non-commercial purposes is hereby granted   *\
//* without fee, provided that the above copyright notice appears in all   *\
//* copies and that both the copyright notice and this permission notice   *\
//* appear in the supporting documentation. The authors make no claims     *\
//* about the suitability of this software for any purpose. It is          *\
//* provided "as is" without express or implied warranty.                  *\
//**************************************************************************

/// \file GPUDisplayBackendOpenGLMagneticField.cxx
/// \author Piotr Nowakowski

#ifdef GPUCA_DISPLAY_GL3W
#include "GL/gl3w.h"
#else
#include <GL/glew.h>
#endif
#if __has_include(<GL/glu.h>)
#include <GL/glu.h>
#else
#define gluErrorString(err) ""
#endif
#ifndef GPUCA_NO_FMT
#include <fmt/printf.h>
#endif

#include "GPUCommonDef.h"
#include "GPUDisplayMagneticField.h"
#include "backend/GPUDisplayBackendOpenGL.h"
#include "shaders/GPUDisplayShaders.h"
#include "GPUDisplay.h"

using namespace GPUCA_NAMESPACE::gpu;

// Runtime minimum version defined in GPUDisplayFrontend.h, keep in sync!
#define GPUCA_BUILD_EVENT_DISPLAY_OPENGL
#if !defined(GL_VERSION_4_5) || GL_VERSION_4_5 != 1
#ifdef GPUCA_STANDALONE
// #error Unsupported OpenGL version < 4.5
#elif defined(GPUCA_O2_LIB)
#pragma message "Unsupported OpenGL version < 4.5, disabling standalone event display"
#else
#warning Unsupported OpenGL version < 4.5, disabling standalone event display
#endif
#undef GPUCA_BUILD_EVENT_DISPLAY_OPENGL
#endif

#ifdef GPUCA_BUILD_EVENT_DISPLAY_OPENGL

#define CHKERR(cmd)                                                                                                 \
  do {                                                                                                              \
    (cmd);                                                                                                          \
    GLenum err = glGetError();                                                                                      \
    while (err != GL_NO_ERROR) {                                                                                    \
      GPUError("OpenGL Error %d: %s (%s: %d)", (int32_t)err, (const char*)gluErrorString(err), __FILE__, __LINE__); \
      throw std::runtime_error("OpenGL Failure");                                                                   \
    }                                                                                                               \
  } while (false)

int32_t GPUDisplayBackendOpenGL::InitMagFieldVisualization()
{
#ifndef GPUCA_NO_FMT
  mMagneticFieldVisualization = std::make_unique<GPUDisplayMagneticField>();
  mMagneticFieldVisualization->generateSeedPoints(mDisplay->cfgL().bFieldLinesCount);

  CHKERR(mVertexShaderPassthrough = glCreateShader(GL_VERTEX_SHADER));
  CHKERR(mGeometryShader = glCreateShader(GL_GEOMETRY_SHADER));

  CHKERR(glShaderSource(mVertexShaderPassthrough, 1, &GPUDisplayShaders::vertexShaderPassthrough, nullptr));
  CHKERR(glCompileShader(mVertexShaderPassthrough));

  const auto constantsExpanded = fmt::format(GPUDisplayShaders::fieldModelShaderConstants,
                                             fmt::arg("dimensions", GPUDisplayMagneticField::DIMENSIONS),
                                             fmt::arg("solZSegs", GPUDisplayMagneticField::MAX_SOLENOID_Z_SEGMENTS),
                                             fmt::arg("solPSegs", GPUDisplayMagneticField::MAX_SOLENOID_P_SEGMENTS),
                                             fmt::arg("solRSegs", GPUDisplayMagneticField::MAX_SOLENOID_R_SEGMENTS),
                                             fmt::arg("solParams", GPUDisplayMagneticField::MAX_SOLENOID_PARAMETERIZATIONS),
                                             fmt::arg("solRows", GPUDisplayMagneticField::MAX_SOLENOID_ROWS),
                                             fmt::arg("solColumns", GPUDisplayMagneticField::MAX_SOLENOID_COLUMNS),
                                             fmt::arg("solCoeffs", GPUDisplayMagneticField::MAX_SOLENOID_COEFFICIENTS),
                                             fmt::arg("dipZSegs", GPUDisplayMagneticField::MAX_DIPOLE_Z_SEGMENTS),
                                             fmt::arg("dipYSegs", GPUDisplayMagneticField::MAX_DIPOLE_Y_SEGMENTS),
                                             fmt::arg("dipXSegs", GPUDisplayMagneticField::MAX_DIPOLE_X_SEGMENTS),
                                             fmt::arg("dipParams", GPUDisplayMagneticField::MAX_DIPOLE_PARAMETERIZATIONS),
                                             fmt::arg("dipRows", GPUDisplayMagneticField::MAX_DIPOLE_ROWS),
                                             fmt::arg("dipColumns", GPUDisplayMagneticField::MAX_DIPOLE_COLUMNS),
                                             fmt::arg("dipCoeffs", GPUDisplayMagneticField::MAX_DIPOLE_COEFFICIENTS),
                                             fmt::arg("maxChebOrder", GPUDisplayMagneticField::MAX_CHEBYSHEV_ORDER));

  std::array geomShaderSource = {GPUDisplayShaders::geometryShaderP1, constantsExpanded.c_str(), GPUDisplayShaders::fieldModelShaderCode, GPUDisplayShaders::geometryShaderP2};

  CHKERR(glShaderSource(mGeometryShader, geomShaderSource.size(), geomShaderSource.data(), nullptr));
  CHKERR(glCompileShader(mGeometryShader));

  if (checkShaderStatus(mVertexShaderPassthrough) || checkShaderStatus(mGeometryShader)) {
    return 1;
  }

  CHKERR(mShaderProgramField = glCreateProgram());
  CHKERR(glAttachShader(mShaderProgramField, mVertexShaderPassthrough));
  CHKERR(glAttachShader(mShaderProgramField, mGeometryShader));
  CHKERR(glAttachShader(mShaderProgramField, mFragmentShader));
  CHKERR(glLinkProgram(mShaderProgramField));

  if (checkProgramStatus(mShaderProgramField)) {
    return 1;
  }

  const auto ATTRIB_ZERO = 0;
  const auto BUFFER_IDX = 0;

  CHKERR(glCreateVertexArrays(1, &VAO_field));
  CHKERR(glEnableVertexArrayAttrib(VAO_field, ATTRIB_ZERO));
  CHKERR(glVertexArrayAttribFormat(VAO_field, ATTRIB_ZERO, 3, GL_FLOAT, GL_FALSE, 0));

  CHKERR(glCreateBuffers(1, &VBO_field));
  CHKERR(glNamedBufferData(VBO_field, mMagneticFieldVisualization->mFieldLineSeedPoints.size() * sizeof(GPUDisplayMagneticField::vtx), mMagneticFieldVisualization->mFieldLineSeedPoints.data(), GL_STATIC_DRAW));

  CHKERR(glVertexArrayVertexBuffer(VAO_field, BUFFER_IDX, VBO_field, 0, sizeof(GPUDisplayMagneticField::vtx)));

  CHKERR(glVertexArrayAttribBinding(VAO_field, ATTRIB_ZERO, BUFFER_IDX));

  CHKERR(glCreateBuffers(1, &mFieldModelViewBuffer));
  CHKERR(glNamedBufferData(mFieldModelViewBuffer, sizeof(hmm_mat4), nullptr, GL_STREAM_DRAW));

  CHKERR(glCreateBuffers(1, &mFieldModelConstantsBuffer));
  CHKERR(glNamedBufferData(mFieldModelConstantsBuffer, sizeof(GPUDisplayMagneticField::RenderConstantsUniform), mMagneticFieldVisualization->mRenderConstantsUniform.get(), GL_STREAM_DRAW));

  CHKERR(glCreateBuffers(1, &mSolenoidSegmentsBuffer));
  CHKERR(glNamedBufferData(mSolenoidSegmentsBuffer, sizeof(GPUDisplayMagneticField::SolenoidSegmentsUniform), mMagneticFieldVisualization->mSolenoidSegments.get(), GL_STREAM_DRAW));
  CHKERR(glCreateBuffers(1, &mSolenoidParameterizationBuffer));
  CHKERR(glNamedBufferData(mSolenoidParameterizationBuffer, sizeof(GPUDisplayMagneticField::SolenoidParameterizationUniform), mMagneticFieldVisualization->mSolenoidParameterization.get(), GL_STREAM_DRAW));

  CHKERR(glCreateBuffers(1, &mDipoleSegmentsBuffer));
  CHKERR(glNamedBufferData(mDipoleSegmentsBuffer, sizeof(GPUDisplayMagneticField::DipoleSegmentsUniform), mMagneticFieldVisualization->mDipoleSegments.get(), GL_STREAM_DRAW));
  CHKERR(glCreateBuffers(1, &mDipoleParameterizationBuffer));
  CHKERR(glNamedBufferData(mDipoleParameterizationBuffer, sizeof(GPUDisplayMagneticField::DipoleParameterizationUniform), mMagneticFieldVisualization->mDipoleParameterization.get(), GL_STREAM_DRAW));
#else
  throw std::runtime_error("Magnetic field needs fmt");
#endif

  return 0;
}

uint32_t GPUDisplayBackendOpenGL::drawField()
{
  if (!mMagneticFieldVisualization) {
    return InitMagFieldVisualization(); // next frame will fill MVP matrix
  }

  if (mMagneticFieldVisualization->mFieldLineSeedPoints.size() != (uint32_t)mDisplay->cfgL().bFieldLinesCount) {
    mMagneticFieldVisualization->generateSeedPoints(mDisplay->cfgL().bFieldLinesCount);
    CHKERR(glNamedBufferData(VBO_field, mMagneticFieldVisualization->mFieldLineSeedPoints.size() * sizeof(GPUDisplayMagneticField::vtx), mMagneticFieldVisualization->mFieldLineSeedPoints.data(), GL_STATIC_DRAW));
  }

  mMagneticFieldVisualization->mRenderConstantsUniform->StepSize = mDisplay->cfgL().bFieldStepSize;
  mMagneticFieldVisualization->mRenderConstantsUniform->StepCount = mDisplay->cfgL().bFieldStepCount;
  CHKERR(glNamedBufferSubData(mFieldModelConstantsBuffer, 0, sizeof(GPUDisplayMagneticField::RenderConstantsUniform), mMagneticFieldVisualization->mRenderConstantsUniform.get()));

  CHKERR(glBindVertexArray(VAO_field));
  CHKERR(glUseProgram(mShaderProgramField));
  CHKERR(glBindBufferBase(GL_UNIFORM_BUFFER, 0, mFieldModelViewBuffer));
  const std::array<float, 4> drawColor = {1.f, 0.f, 0.f, 1.f};
  const auto color = glGetUniformLocation(mShaderProgramField, "color");
  CHKERR(glUniform4fv(color, 1, drawColor.data()));

  CHKERR(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mFieldModelConstantsBuffer));
  CHKERR(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mSolenoidSegmentsBuffer));
  CHKERR(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mDipoleSegmentsBuffer));
  CHKERR(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mSolenoidParameterizationBuffer));
  CHKERR(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, mDipoleParameterizationBuffer));

  CHKERR(glDrawArrays(GL_POINTS, 0, mMagneticFieldVisualization->mFieldLineSeedPoints.size()));

  CHKERR(glUseProgram(0));
  CHKERR(glBindVertexArray(0));

  return 0;
}

void GPUDisplayBackendOpenGL::ExitMagFieldVisualization()
{
  CHKERR(glDeleteBuffers(1, &mFieldModelViewBuffer));
  CHKERR(glDeleteBuffers(1, &mFieldModelConstantsBuffer));
  CHKERR(glDeleteBuffers(1, &mSolenoidSegmentsBuffer));
  CHKERR(glDeleteBuffers(1, &mSolenoidParameterizationBuffer));
  CHKERR(glDeleteBuffers(1, &mDipoleSegmentsBuffer));
  CHKERR(glDeleteBuffers(1, &mDipoleParameterizationBuffer));
  CHKERR(glDeleteProgram(mShaderProgramField));
  CHKERR(glDeleteShader(mGeometryShader));
  CHKERR(glDeleteShader(mVertexShaderPassthrough));
  CHKERR(glDeleteBuffers(1, &VBO_field));
  CHKERR(glDeleteVertexArrays(1, &VAO_field));
}

#else  // GPUCA_BUILD_EVENT_DISPLAY_OPENGL
int32_t GPUDisplayBackendOpenGL::InitMagFieldVisualization()
{
  return 0;
}
uint32_t GPUDisplayBackendOpenGL::drawField() { return 0; }
void GPUDisplayBackendOpenGL::ExitMagFieldVisualization() {}
#endif // GPUCA_BUILD_EVENT_DISPLAY_OPENGL

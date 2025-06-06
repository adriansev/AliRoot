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

/// \file GPUDisplayShaders.h
/// \author David Rohr

#ifndef GPUDISPLAYSHADERS_H
#define GPUDISPLAYSHADERS_H

#include "GPUCommonDef.h"
namespace GPUCA_NAMESPACE
{
namespace gpu
{

struct GPUDisplayShaders {
  static constexpr const char* vertexShader = R"(
#version 450 core
layout (location = 0) in vec3 pos;
uniform mat4 ModelViewProj;

void main()
{
  gl_Position = ModelViewProj * vec4(pos.x, pos.y, pos.z, 1.0);
}
)";

  static constexpr const char* fragmentShader = R"(
#version 450 core
out vec4 fragColor;
uniform vec4 color;

void main()
{
  fragColor = vec4(color.x, color.y, color.z, 1.f);
}
)";

  static constexpr const char* vertexShaderTexture = R"(
#version 450 core
layout (location = 0) in vec4 vertex;
out vec2 TexCoords;

uniform mat4 projection;

void main()
{
  gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
  TexCoords = vertex.zw;
}
)";

  static constexpr const char* fragmentShaderTexture = R"(
#version 450 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D tex;
uniform float alpha;

void main()
{
    color = vec4(texture(tex, TexCoords).rgb, alpha);
}
)";

  static constexpr const char* fragmentShaderText = R"(
#version 450 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;
uniform vec4 textColor;

void main()
{
  vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
  color = textColor * sampled;
}
)";

  static constexpr const char* vertexShaderPassthrough = R"(
#version 450 core
layout (location = 0) in vec3 pos;
void main()
{
  gl_Position = vec4(pos, 1.0);
}
)";

  static constexpr const char* geometryShaderP1 = R"(
  #version 450 core
)";

  static constexpr const char* fieldModelShaderConstants = R"(
#define DIMENSIONS {dimensions}

#define SOL_Z_SEGS {solZSegs}
#define SOL_P_SEGS {solPSegs}
#define SOL_R_SEGS {solRSegs}

#define SOL_PARAMS {solParams}
#define SOL_ROWS {solRows}
#define SOL_COLUMNS {solColumns}
#define SOL_COEFFS {solCoeffs}

#define DIP_Z_SEGS {dipZSegs}
#define DIP_Y_SEGS {dipYSegs}
#define DIP_X_SEGS {dipXSegs}

#define DIP_PARAMS {dipParams}
#define DIP_ROWS {dipRows}
#define DIP_COLUMNS {dipColumns}
#define DIP_COEFFS {dipCoeffs}

#define MAX_CHEB_ORDER {maxChebOrder}
)";

  static constexpr const char* fieldModelShaderCode = R"(
layout(std430, binding = 0) restrict readonly buffer field_config_ssbo {
    uint32_t StepCount;
    float StepSize;
} field_config;

layout(std430, binding = 1) restrict readonly buffer sol_segment_ssbo {
    float MinZ;
    float MaxZ;
    float MultiplicativeFactor;

    int32_t ZSegments;

    float SegZSol[SOL_Z_SEGS];

    int32_t BegSegPSol[SOL_Z_SEGS];
    int32_t NSegPSol[SOL_Z_SEGS];

    float SegPSol[SOL_P_SEGS];
    int32_t BegSegRSol[SOL_P_SEGS];
    int32_t NSegRSol[SOL_P_SEGS];

    float SegRSol[SOL_R_SEGS];
    int32_t SegIDSol[SOL_R_SEGS];
} sol_segment;

layout(std430, binding = 2) restrict readonly buffer dip_segment_ssbo {
    float MinZ;
    float MaxZ;
    float MultiplicativeFactor;

    int32_t ZSegments;

    float SegZDip[DIP_Z_SEGS];

    int32_t BegSegYDip[DIP_Z_SEGS];
    int32_t NSegYDip[DIP_Z_SEGS];

    float SegYDip[DIP_Y_SEGS];
    int32_t BegSegXDip[DIP_Y_SEGS];
    int32_t NSegXDip[DIP_Y_SEGS];

    float SegXDip[DIP_X_SEGS];
    int32_t SegIDDip[DIP_X_SEGS];
} dip_segment;

layout(std430, binding = 3) restrict readonly buffer sol_params_ssbo {
    float BOffsets[DIMENSIONS*SOL_PARAMS];
    float BScales[DIMENSIONS*SOL_PARAMS];
    float BMin[DIMENSIONS*SOL_PARAMS];
    float BMax[DIMENSIONS*SOL_PARAMS];
    int32_t NRows[DIMENSIONS*SOL_PARAMS];
    int32_t ColsAtRowOffset[DIMENSIONS*SOL_PARAMS];
    int32_t CofsAtRowOffset[DIMENSIONS*SOL_PARAMS];

    int32_t NColsAtRow[SOL_ROWS];
    int32_t CofsAtColOffset[SOL_ROWS];

    int32_t NCofsAtCol[SOL_COLUMNS];
    int32_t AtColCoefOffset[SOL_COLUMNS];

    float Coeffs[SOL_COEFFS];
} sol_params;

layout(std430, binding = 4) restrict readonly buffer dip_params_ssbo {
    float BOffsets[DIMENSIONS*DIP_PARAMS];
    float BScales[DIMENSIONS*DIP_PARAMS];
    float BMin[DIMENSIONS*DIP_PARAMS];
    float BMax[DIMENSIONS*DIP_PARAMS];
    int32_t NRows[DIMENSIONS*DIP_PARAMS];
    int32_t ColsAtRowOffset[DIMENSIONS*DIP_PARAMS];
    int32_t CofsAtRowOffset[DIMENSIONS*DIP_PARAMS];

    int32_t NColsAtRow[DIP_ROWS];
    int32_t CofsAtColOffset[DIP_ROWS];

    int32_t NCofsAtCol[DIP_COLUMNS];
    int32_t AtColCoefOffset[DIP_COLUMNS];

    float Coeffs[DIP_COEFFS];
} dip_params;

float tmpCfs1[MAX_CHEB_ORDER];
float tmpCfs0[MAX_CHEB_ORDER];

vec3 CarttoCyl(vec3 pos) {
    return vec3(length(pos.xy), atan(pos.y, pos.x), pos.z);
}

int32_t findSolSegment(vec3 pos) {
    int32_t rid,pid,zid;

    for(zid=0; zid < sol_segment.ZSegments; zid++) if(pos.z<sol_segment.SegZSol[zid]) break;
    if(--zid < 0) zid = 0;

    const int32_t psegBeg = sol_segment.BegSegPSol[zid];
    for(pid=0; pid<sol_segment.NSegPSol[zid]; pid++) if(pos.y<sol_segment.SegPSol[psegBeg+pid]) break;
    if(--pid < 0) pid = 0;
    pid += psegBeg;

    const int32_t rsegBeg = sol_segment.BegSegRSol[pid];
    for(rid=0; rid<sol_segment.NSegRSol[pid]; rid++) if(pos.x<sol_segment.SegRSol[rsegBeg+rid]) break;
    if(--rid < 0) rid = 0;
    rid += rsegBeg;

    return sol_segment.SegIDSol[rid];
}

int32_t findDipSegment(vec3 pos) {
    int32_t xid,yid,zid;

    for(zid=0; zid < dip_segment.ZSegments; zid++) if(pos.z<dip_segment.SegZDip[zid]) break;
    if(--zid < 0) zid = 0;

    const int32_t ysegBeg = dip_segment.BegSegYDip[zid];
    for(yid=0; yid<dip_segment.NSegYDip[zid]; yid++) if(pos.y<dip_segment.SegYDip[ysegBeg+yid]) break;
    if(--yid < 0) yid = 0;
    yid += ysegBeg;

    const int32_t xsegBeg = dip_segment.BegSegXDip[yid];
    for(xid=0; xid<dip_segment.NSegXDip[yid]; xid++) if(pos.x<dip_segment.SegXDip[xsegBeg+xid]) break;
    if(--xid < 0) xid = 0;
    xid += xsegBeg;

    return dip_segment.SegIDDip[xid];
}

vec3 mapToInternalSol(int32_t segID, vec3 rphiz) {
    const int32_t index = DIMENSIONS*segID;
    vec3 offsets = vec3(sol_params.BOffsets[index+0], sol_params.BOffsets[index+1], sol_params.BOffsets[index+2]);
    vec3 scales = vec3(sol_params.BScales[index+0], sol_params.BScales[index+1], sol_params.BScales[index+2]);

    return (rphiz-offsets)*scales;
}

vec3 mapToInternalDip(int32_t segID, vec3 pos) {
    const int32_t index = DIMENSIONS*segID;
    const vec3 offsets = vec3(dip_params.BOffsets[index+0], dip_params.BOffsets[index+1], dip_params.BOffsets[index+2]);
    const vec3 scales = vec3(dip_params.BScales[index+0], dip_params.BScales[index+1], dip_params.BScales[index+2]);

    return (pos-offsets)*scales;
}

float cheb1DArray(float x, float arr[MAX_CHEB_ORDER], int32_t ncf) {
    if(ncf <= 0) return 0.0f;

    const float x2 = 2*x;

    vec3 b = vec3(arr[--ncf], 0, 0);
    --ncf;

    const vec3 t1 = vec3(1, x2, -1);

    for (int32_t i=ncf;i>=0;i--) {
        b.zy = b.yx;
        b.x = arr[i];
        b.x = dot(t1, b);
    }

    const vec3 t = vec3(1, -x, 0);
    return dot(t, b);
}

float cheb1DParamsSol(float x, int32_t coeff_offset, int32_t ncf) {
    if(ncf <= 0) return 0.0f;

    const float x2 = 2*x;

    vec3 b = vec3(sol_params.Coeffs[coeff_offset + (--ncf)], 0, 0);
    --ncf;

    const vec3 t1 = vec3(1, x2, -1);

    for (int32_t i=ncf;i>=0;i--) {
        b.zy = b.yx;
        b.x = sol_params.Coeffs[coeff_offset + i];
        b.x = dot(t1, b);
    }

    const vec3 t = vec3(1, -x, 0);
    return dot(t, b);
}

float cheb1DParamsDip(float x, int32_t coeff_offset, int32_t ncf) {
    if(ncf <= 0) return 0.0f;

    const float x2 = 2*x;

    vec3 b = vec3(dip_params.Coeffs[coeff_offset + (--ncf)], 0, 0);
    --ncf;

    const vec3 t1 = vec3(1, x2, -1);

    for (int32_t i=ncf;i>=0;i--) {
        b.zy = b.yx;
        b.x = dip_params.Coeffs[coeff_offset + i];
        b.x = dot(t1, b);
    }

    const vec3 t = vec3(1, -x, 0);
    return dot(t, b);
}

bool IsBetween(vec3 sMin, vec3 val, vec3 sMax) {
    return all(lessThanEqual(sMin, val)) && all(lessThanEqual(val, sMax));
}

bool IsInsideSol(int32_t segID, vec3 rphiz) {
    const int32_t index = DIMENSIONS*segID;

    const vec3 seg_min = vec3(sol_params.BMin[index+0], sol_params.BMin[index+1], sol_params.BMin[index+2]);
    const vec3 seg_max = vec3(sol_params.BMax[index+0], sol_params.BMax[index+1], sol_params.BMax[index+2]);

    return IsBetween(seg_min, rphiz, seg_max);
}

bool IsInsideDip(int32_t segID, vec3 pos) {
    const int32_t index = DIMENSIONS*segID;

    const vec3 seg_min = vec3(dip_params.BMin[index+0], dip_params.BMin[index+1], dip_params.BMin[index+2]);
    const vec3 seg_max = vec3(dip_params.BMax[index+0], dip_params.BMax[index+1], dip_params.BMax[index+2]);

    return IsBetween(seg_min, pos, seg_max);
}

float Eval3DSol(int32_t segID, int32_t dim, vec3 internal) {
    const int32_t index = DIMENSIONS*segID;
    const int32_t n_rows = sol_params.NRows[index+dim];
    const int32_t cols_at_row_offset = sol_params.ColsAtRowOffset[index+dim];
    const int32_t coeffs_at_row_offset = sol_params.CofsAtRowOffset[index+dim];

    for(int32_t row = 0; row < n_rows; row++) {
        const int32_t n_cols = sol_params.NColsAtRow[cols_at_row_offset+row];
        const int32_t coeff_at_col_offset = sol_params.CofsAtColOffset[cols_at_row_offset+row];

        for(int32_t col = 0; col < n_cols; col++) {
            const int32_t n_coeffs = sol_params.NCofsAtCol[coeff_at_col_offset+col];
            const int32_t per_col_coeff_offset = sol_params.AtColCoefOffset[coeff_at_col_offset+col];

            const int32_t coeffs_offset = coeffs_at_row_offset + per_col_coeff_offset;

            tmpCfs1[col] = cheb1DParamsSol(internal.z, coeffs_offset,n_coeffs);
        }
        tmpCfs0[row] = cheb1DArray(internal.y, tmpCfs1, n_cols);
    }

    return cheb1DArray(internal.x, tmpCfs0, n_rows);
}

vec3 EvalSol(int32_t segID, vec3 rphiz) {
    const vec3 internal = mapToInternalSol(segID, rphiz);
    return vec3(Eval3DSol(segID, 0, internal), Eval3DSol(segID, 1, internal), Eval3DSol(segID, 2, internal));
}

float Eval3DDip(int32_t segID, int32_t dim, vec3 internal) {
    const int32_t index = DIMENSIONS*segID;
    const int32_t n_rows = dip_params.NRows[index+dim];
    const int32_t cols_at_row_offset = dip_params.ColsAtRowOffset[index+dim];
    const int32_t coeffs_at_row_offset = dip_params.CofsAtRowOffset[index+dim];

    for(int32_t row = 0; row < n_rows; row++) {
        const int32_t n_cols = dip_params.NColsAtRow[cols_at_row_offset+row];
        const int32_t coeff_at_col_offset = dip_params.CofsAtColOffset[cols_at_row_offset+row];

        for(int32_t col = 0; col < n_cols; col++) {
            const int32_t n_coeffs = dip_params.NCofsAtCol[coeff_at_col_offset+col];
            const int32_t per_col_coeff_offset = dip_params.AtColCoefOffset[coeff_at_col_offset+col];

            const int32_t coeffs_offset = coeffs_at_row_offset + per_col_coeff_offset;

            tmpCfs1[col] = cheb1DParamsDip(internal.z, coeffs_offset, n_coeffs);
        }
        tmpCfs0[row] = cheb1DArray(internal.y, tmpCfs1, n_cols);
    }

    return cheb1DArray(internal.x, tmpCfs0, n_rows);
}

vec3 EvalDip(int32_t segID, vec3 pos) {
    const vec3 internal = mapToInternalDip(segID, pos);
    return vec3(Eval3DDip(segID, 0, internal), Eval3DDip(segID, 1, internal), Eval3DDip(segID, 2, internal));
}

vec3 CyltoCartCylB(vec3 rphiz, vec3 brphiz) {
    const float btr = length(brphiz.xy);
    const float psiPLUSphi = atan(brphiz.y, brphiz.x) + rphiz.y;

    return vec3(btr*cos(psiPLUSphi), btr*sin(psiPLUSphi), brphiz.z);
}

vec3 MachineField(vec3 pos) {
    return vec3(0);
}

vec3 SolDipField(vec3 pos) {
    if(pos.z > sol_segment.MinZ) {
        const vec3 rphiz = CarttoCyl(pos);
        const int32_t segID = findSolSegment(rphiz);
        if(segID >=0 && IsInsideSol(segID, rphiz)) {
            const vec3 brphiz = EvalSol(segID, rphiz);
            return CyltoCartCylB(rphiz, brphiz) * sol_segment.MultiplicativeFactor;
        }
    }

    const int32_t segID = findDipSegment(pos);
    if(segID >= 0 && IsInsideDip(segID, pos)) {
        return EvalDip(segID, pos) * dip_segment.MultiplicativeFactor;
    }

    return vec3(0);
}

const float MinZ = dip_segment.MinZ;
const float MaxZ = sol_segment.MaxZ;

vec3 Field(vec3 pos) {
    if(pos.z > MinZ && pos.z < MaxZ) {
        return SolDipField(pos);
    }
    return vec3(0);
}
)";

  static constexpr const char* geometryShaderP2 = R"(
layout (points) in;
layout (line_strip, max_vertices = 256) out;

layout (binding = 0) uniform uniformMatrix { mat4 ModelViewProj; } um;

const float positionScale = 100.0f;

void main() {
    vec3 position = gl_in[0].gl_Position.xyz;

    for(uint32_t i = 0; i < field_config.StepCount; ++i) {
        gl_Position = um.ModelViewProj * vec4(position/positionScale, 1.0f);
        EmitVertex();
        const vec3 b_vec = Field(position);
        position -= b_vec * field_config.StepSize;
    }
    EndPrimitive();
}
)";
};
} // namespace gpu
} // namespace GPUCA_NAMESPACE

#endif

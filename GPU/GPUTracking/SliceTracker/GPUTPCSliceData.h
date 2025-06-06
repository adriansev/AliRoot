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

/// \file GPUTPCSliceData.h
/// \author Matthias Kretz, Sergey Gorbunov, David Rohr

#ifndef GPUTPCSLICEDATA_H
#define GPUTPCSLICEDATA_H

#include "GPUTPCDef.h"
#include "GPUTPCRow.h"
#include "GPUCommonMath.h"
#include "GPUParam.h"
#include "GPUProcessor.h"

namespace GPUCA_NAMESPACE
{
namespace gpu
{
struct GPUTPCClusterData;
class GPUTPCHit;

class GPUTPCSliceData
{
 public:
  GPUTPCSliceData() : mNumberOfHits(0), mNumberOfHitsPlusAlign(0), mClusterIdOffset(0), mGPUTextureBase(nullptr), mRows(nullptr), mLinkUpData(nullptr), mLinkDownData(nullptr), mClusterData(nullptr) {}

#ifndef GPUCA_GPUCODE_DEVICE
  ~GPUTPCSliceData() = default;
  void InitializeRows(const GPUParam& p);
  void SetMaxData();
  void SetClusterData(const GPUTPCClusterData* data, int32_t nClusters, int32_t clusterIdOffset);
  void* SetPointersInput(void* mem, bool idsOnGPU, bool sliceDataOnGPU);
  void* SetPointersScratch(void* mem, bool idsOnGPU, bool sliceDataOnGPU);
  void* SetPointersLinks(void* mem);
  void* SetPointersWeights(void* mem);
  void* SetPointersClusterIds(void* mem, bool idsOnGPU);
  void* SetPointersRows(void* mem);
#endif

  GPUd() int32_t InitFromClusterData(int32_t nBlocks, int32_t nThreads, int32_t iBlock, int32_t iThread, GPUconstantref() const GPUConstantMem* mem, int32_t iSlice, float* tmpMinMax);

  /**
 * Return the number of hits in this slice.
 */
  GPUhd() int32_t NumberOfHits() const { return mNumberOfHits; }
  GPUhd() int32_t NumberOfHitsPlusAlign() const { return mNumberOfHitsPlusAlign; }
  GPUhd() int32_t ClusterIdOffset() const { return mClusterIdOffset; }

  /**
 * Access to the hit links.
 *
 * The links values give the hit index in the row above/below. Or -1 if there is no link.
 */
  GPUd() calink HitLinkUpData(const GPUTPCRow& row, const calink& hitIndex) const;
  GPUd() calink HitLinkDownData(const GPUTPCRow& row, const calink& hitIndex) const;

  GPUhdi() GPUglobalref() const cahit2* HitData(const GPUTPCRow& row) const { return &mHitData[row.mHitNumberOffset]; }
  GPUhdi() GPUglobalref() cahit2* HitData(const GPUTPCRow& row) { return &mHitData[row.mHitNumberOffset]; }
  GPUhd() GPUglobalref() const cahit2* HitData() const { return (mHitData); }
  GPUdi() GPUglobalref() const calink* HitLinkUpData(const GPUTPCRow& row) const { return &mLinkUpData[row.mHitNumberOffset]; }
  GPUdi() GPUglobalref() calink* HitLinkUpData(const GPUTPCRow& row) { return &mLinkUpData[row.mHitNumberOffset]; }
  GPUdi() GPUglobalref() const calink* HitLinkDownData(const GPUTPCRow& row) const { return &mLinkDownData[row.mHitNumberOffset]; }
  GPUdi() GPUglobalref() const calink* FirstHitInBin(const GPUTPCRow& row) const { return &mFirstHitInBin[row.mFirstHitInBinOffset]; }

  GPUd() void SetHitLinkUpData(const GPUTPCRow& row, const calink& hitIndex, const calink& value);
  GPUd() void SetHitLinkDownData(const GPUTPCRow& row, const calink& hitIndex, const calink& value);

  /**
 * Return the y and z coordinate(s) of the given hit(s).
 */
  GPUd() cahit HitDataY(const GPUTPCRow& row, const uint32_t& hitIndex) const;
  GPUd() cahit HitDataZ(const GPUTPCRow& row, const uint32_t& hitIndex) const;
  GPUd() cahit2 HitData(const GPUTPCRow& row, const uint32_t& hitIndex) const;

  /**
 * For a given bin index, content tells how many hits there are in the preceding bins. This maps
 * directly to the hit index in the given row.
 *
 * \param binIndexes in the range 0 to row.Grid.N + row.Grid.Ny + 3.
 */
  GPUd() calink FirstHitInBin(const GPUTPCRow& row, calink binIndex) const;

  /**
 * If the given weight is higher than what is currently stored replace with the new weight.
 */
  GPUd() void MaximizeHitWeight(const GPUTPCRow& row, uint32_t hitIndex, uint32_t weight);
  GPUd() void SetHitWeight(const GPUTPCRow& row, uint32_t hitIndex, uint32_t weight);

  /**
 * Return the maximal weight the given hit got from one tracklet
 */
  GPUd() int32_t HitWeight(const GPUTPCRow& row, uint32_t hitIndex) const;

  /**
 * Returns the index in the original GPUTPCClusterData object of the given hit
 */
  GPUhd() int32_t ClusterDataIndex(const GPUTPCRow& row, uint32_t hitIndex) const;
  GPUd() GPUglobalref() const int32_t* ClusterDataIndex() const { return mClusterDataIndex; }
  GPUd() GPUglobalref() int32_t* ClusterDataIndex() { return mClusterDataIndex; }

  /**
 * Return the row object for the given row index.
 */
  GPUhdi() GPUglobalref() const GPUTPCRow& Row(int32_t rowIndex) const { return mRows[rowIndex]; }
  GPUhdi() GPUglobalref() GPUTPCRow* Rows() const { return mRows; }

  GPUhdi() GPUglobalref() GPUAtomic(uint32_t) * HitWeights() { return (mHitWeights); }

  GPUhdi() void SetGPUTextureBase(GPUglobalref() const void* val) { mGPUTextureBase = val; }
  GPUhdi() char* GPUTextureBase() const { return ((char*)mGPUTextureBase); }
  GPUhdi() char* GPUTextureBaseConst() const { return ((char*)mGPUTextureBase); }

  GPUhdi() GPUglobalref() const GPUTPCClusterData* ClusterData() const { return mClusterData; }

 private:
#ifndef GPUCA_GPUCODE
  GPUTPCSliceData& operator=(const GPUTPCSliceData&) = delete; // ROOT 5 tries to use this if it is not private
  GPUTPCSliceData(const GPUTPCSliceData&) = delete;            //
#endif
  GPUd() void CreateGrid(GPUconstantref() const GPUConstantMem* mem, GPUTPCRow* GPUrestrict() row, float yMin, float yMax, float zMin, float zMax);
  GPUd() void SetRowGridEmpty(GPUTPCRow& GPUrestrict() row);
  GPUd() static void GetMaxNBins(GPUconstantref() const GPUConstantMem* mem, GPUTPCRow* GPUrestrict() row, int32_t& maxY, int32_t& maxZ);
  GPUd() uint32_t GetGridSize(uint32_t nHits, uint32_t nRows);

  friend class GPUTPCNeighboursFinder;
  friend class GPUTPCStartHitsFinder;

  int32_t mNumberOfHits; // the number of hits in this slice
  int32_t mNumberOfHitsPlusAlign;
  int32_t mClusterIdOffset;

  GPUglobalref() const void* mGPUTextureBase; // pointer to start of GPU texture

  GPUglobalref() GPUTPCRow* mRows; // The row objects needed for most accessor functions

  GPUglobalref() calink* mLinkUpData;    // hit index in the row above which is linked to the given (global) hit index
  GPUglobalref() calink* mLinkDownData;  // hit index in the row below which is linked to the given (global) hit index
  GPUglobalref() cahit2* mHitData;       // packed y,z coordinate of the given (global) hit index
  GPUglobalref() int32_t* mClusterDataIndex; // see ClusterDataIndex()

  /*
 * The size of the array is row.Grid.N + row.Grid.Ny + 3. The row.Grid.Ny + 3 is an optimization
 * to remove the need for bounds checking. The last values are the same as the entry at [N - 1].
 */
  GPUglobalref() calink* mFirstHitInBin;                // see FirstHitInBin
  GPUglobalref() GPUAtomic(uint32_t) * mHitWeights;     // the weight of the longest tracklet crossed the cluster
  GPUglobalref() const GPUTPCClusterData* mClusterData;
};

GPUdi() calink GPUTPCSliceData::HitLinkUpData(const GPUTPCRow& row, const calink& hitIndex) const { return mLinkUpData[row.mHitNumberOffset + hitIndex]; }

GPUdi() calink GPUTPCSliceData::HitLinkDownData(const GPUTPCRow& row, const calink& hitIndex) const { return mLinkDownData[row.mHitNumberOffset + hitIndex]; }

GPUdi() void GPUTPCSliceData::SetHitLinkUpData(const GPUTPCRow& row, const calink& hitIndex, const calink& value)
{
  mLinkUpData[row.mHitNumberOffset + hitIndex] = value;
}

GPUdi() void GPUTPCSliceData::SetHitLinkDownData(const GPUTPCRow& row, const calink& hitIndex, const calink& value)
{
  mLinkDownData[row.mHitNumberOffset + hitIndex] = value;
}

GPUdi() cahit GPUTPCSliceData::HitDataY(const GPUTPCRow& row, const uint32_t& hitIndex) const { return mHitData[row.mHitNumberOffset + hitIndex].x; }

GPUdi() cahit GPUTPCSliceData::HitDataZ(const GPUTPCRow& row, const uint32_t& hitIndex) const { return mHitData[row.mHitNumberOffset + hitIndex].y; }

GPUdi() cahit2 GPUTPCSliceData::HitData(const GPUTPCRow& row, const uint32_t& hitIndex) const { return mHitData[row.mHitNumberOffset + hitIndex]; }

GPUdi() calink GPUTPCSliceData::FirstHitInBin(const GPUTPCRow& row, calink binIndex) const { return mFirstHitInBin[row.mFirstHitInBinOffset + binIndex]; }

GPUhdi() int32_t GPUTPCSliceData::ClusterDataIndex(const GPUTPCRow& row, uint32_t hitIndex) const { return mClusterDataIndex[row.mHitNumberOffset + hitIndex]; }

GPUdi() void GPUTPCSliceData::MaximizeHitWeight(const GPUTPCRow& row, uint32_t hitIndex, uint32_t weight)
{
  CAMath::AtomicMax(&mHitWeights[row.mHitNumberOffset + hitIndex], weight);
}

GPUdi() void GPUTPCSliceData::SetHitWeight(const GPUTPCRow& row, uint32_t hitIndex, uint32_t weight)
{
  mHitWeights[row.mHitNumberOffset + hitIndex] = weight;
}

GPUdi() int32_t GPUTPCSliceData::HitWeight(const GPUTPCRow& row, uint32_t hitIndex) const { return mHitWeights[row.mHitNumberOffset + hitIndex]; }
} // namespace gpu
} // namespace GPUCA_NAMESPACE

#endif // GPUTPCSLICEDATA_H

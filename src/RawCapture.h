/*
 * RawCapture.h
 *
 * File format of the raw accelerometer recordings, next to the data log of a session:
 *
 *   R_HHMMSS_NN.bin   capture on demand ("rq raw <s>" / /debug/imu), continuous 400 Hz
 *   S_HHMMSS.bin      shock snippets: 0.25 s before to 0.5 s after every logged shock
 *
 * (HHMMSS = the session's data log L_HHMMSS.bin, NN = capture number since boot; without
 * a clock the NO_TIME counter takes the place of HHMMSS, as for the other logs.)
 *
 * Layout: one FileHeader, then blocks. A block is a BlockHeader followed by count frames of
 * int16 x, y, z -- the sensor's raw LSB, NOT scale-corrected (header.scale and lsbPerG give
 * g), so a later re-calibration can still be applied. All little-endian, no padding.
 *
 * Readers: Tools/bikelog/raw.py (Python), Tools/rqreplay/rq_replay.cpp (runs the firmware's
 * RoadQuality on a recording). Change all three together and bump VERSION.
 *
 * Size: 400 Hz * 6 byte = 2400 byte/s plus 24 byte per block (one block per FIFO chunk of
 * up to 20 frames) -- about 150 KB per minute of capture, ~1.8 KB per shock snippet.
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace RawCap {

static constexpr uint8_t VERSION = 1;
static constexpr uint16_t SYNC = 0xB10C;			// first two bytes of every block (0C B1 on disk)
static constexpr uint16_t SPEED_UNKNOWN = 0xFFFF;
static constexpr uint16_t SPEED_AGE_UNKNOWN = 0xFFFF;

enum Kind : uint8_t {
	KIND_CAPTURE = 0,
	KIND_SNIPPETS = 1,
};

enum BlockType : uint8_t {
	BLOCK_CONTINUOUS = 0,
	BLOCK_SHOCK = 1,
};

enum BlockFlags : uint8_t {
	BF_GAP_BEFORE     = 0x01,	// samples are missing between the previous block and this one
	BF_SPEED_FROM_GPS = 0x02,	// speedCms is the phone's GPS speed, not the wheel sensor's
};

struct FileHeader {
	uint8_t magic[4];					//  0  'B','C','R','W'
	uint8_t version;					//  4  = VERSION
	uint8_t kind;						//  5  Kind
	uint16_t odrHz;						//  6  400
	float lsbPerG;						//  8  2048 (+/-16 g)
	float scale;						// 12  static calibration: g = raw / lsbPerG * scale
	float g0[3];						// 16  calibrated gravity vector at rest (g, before scale)
	float noiseG;						// 28  noise floor
	int64_t startEpochMs;				// 32
	float wheelbaseM;					// 40  setting at the time of recording
	uint8_t intervalS;					// 44  road-quality interval setting
	uint8_t rangeG;						// 45  16
	uint16_t preMs;						// 46  snippets: before the peak
	uint16_t postMs;					// 48  snippets: after the peak
	uint8_t reserved[14];				// 50
};

struct BlockHeader {
	uint16_t sync;						//  0  = SYNC
	uint8_t type;						//  2  BlockType
	uint8_t flags;						//  3  BlockFlags
	uint16_t count;						//  4  frames following
	uint16_t speedCms;					//  6  speed at the block (SPEED_UNKNOWN if none)
	int64_t epochMs;					//  8  wall-clock time of the first frame
	uint32_t ref;						// 16  continuous: block number; shock: LogRec::Shock::eventSeq
	uint16_t speedAgeMs;				// 20  since the last wheel-speed update (SPEED_AGE_UNKNOWN: none)
	uint16_t reserved;					// 22
};

static_assert(sizeof(FileHeader) == 64, "RawCap::FileHeader must be 64 byte");
static_assert(sizeof(BlockHeader) == 24, "RawCap::BlockHeader must be 24 byte");
static_assert(offsetof(FileHeader, startEpochMs) == 32 && offsetof(FileHeader, preMs) == 46, "FileHeader layout");
static_assert(offsetof(BlockHeader, epochMs) == 8 && offsetof(BlockHeader, speedAgeMs) == 20, "BlockHeader layout");

}	// namespace RawCap

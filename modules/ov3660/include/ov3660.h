/*
 * SPDX-License-Identifier: Apache-2.0
 * OV3660 app-visible control IDs (driver-private range).
 */

#ifndef OV3660_H_
#define OV3660_H_

#include <zephyr/drivers/video-controls.h>

/** AE target offset, range −5…+5 (0 = default; negative = darker in bright scenes) */
#define OV3660_CID_AE_LEVEL  (VIDEO_CID_PRIVATE_BASE + 0)
/** Sharpness, range −3…+3 */
#define OV3660_CID_SHARPNESS (VIDEO_CID_PRIVATE_BASE + 1)

#endif /* OV3660_H_ */

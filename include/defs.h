/* SPDX-License-Identifier: MIT */

#ifndef DEFS_H
#define DEFS_H

/*
 * Generic Size Definitions
 * Used for abstract buffer capacities where no specific system limit applies.
 */
#define MEDIUM_BUFFER 128
#define SMALL_BUFFER  64
#define MINI_BUFFER   32
#define TINY_BUFFER   16
#define HEX_BUFFER    8

/*
 * IO Limits
 * Used for semantic limits related to Input/Output operations.
 */
#define LINE_BUFFER   256
#define PATH_BUFFER   512

#endif /* DEFS_H */
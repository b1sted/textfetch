/* SPDX-License-Identifier: MIT */

#ifndef TEST_STUBS_H
#define TEST_STUBS_H

#include <stddef.h>

#define STUB_BUFFER_SIZE 128

extern char stub_identity[STUB_BUFFER_SIZE];
extern char stub_distro[STUB_BUFFER_SIZE];
extern char stub_model[STUB_BUFFER_SIZE];
extern char stub_uptime[STUB_BUFFER_SIZE];
extern char stub_hardware_calls[64];

/* Restores deterministic values used by the platform stubs. */
void stubs_reset(void);

#endif /* TEST_STUBS_H */

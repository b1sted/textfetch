/* SPDX-License-Identifier: MIT */

#ifndef ANDROID_STUB_SYSTEM_PROPERTIES_H
#define ANDROID_STUB_SYSTEM_PROPERTIES_H

/* Bionic limits mirrored for host-side Android tests. */
#define PROP_NAME_MAX 32
#define PROP_VALUE_MAX 92

int __system_property_get(const char *name, char *value);

#endif /* ANDROID_STUB_SYSTEM_PROPERTIES_H */

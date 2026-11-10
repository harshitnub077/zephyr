/*
 * Copyright The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_POSIX_FNMATCH_H_
#define ZEPHYR_INCLUDE_POSIX_FNMATCH_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FNM_NOMATCH  1
#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD   0x04

#ifndef FNM_LEADING_DIR
#define FNM_LEADING_DIR 0x08
#endif
#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD    0x10
#endif
#ifndef FNM_EXTMATCH
#define FNM_EXTMATCH    0x20
#endif
#define FNM_IGNORECASE FNM_CASEFOLD

int fnmatch(const char *pattern, const char *string, int flags);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_POSIX_FNMATCH_H_ */

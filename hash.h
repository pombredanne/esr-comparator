/* interface file for comparator hash function */

/* SPDX-License-Identifier: BSD-2-clause */

#include <stdint.h>

#ifndef FORCE_MD5
typedef uint64_t	hashval_t;
#define HASHMETHOD	"RXOR"
#else
typedef unsigned char	hashval_t[16];
#define HASHMETHOD	"MD5"
#endif

#define hash_compare(s, t)	memcmp(&(s), &(t), sizeof(hashval_t))

void hash_init(void);
void hash_update(unsigned char *buffer, const int len);
void hash_complete(hashval_t *hp);
char *hash_dump(hashval_t hash);

/* hash.h ends */

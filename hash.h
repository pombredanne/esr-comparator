/* interface file for comparator hash function */

#ifdef CUSTOM_HASH
typedef unsigned long long	hashval_t;
#define hash_compare(s, t)	((s) - (t))
#else
typedef unsigned char	hashval_t[16];
#define hash_compare(s, t)	memcmp((s), (t), sizeof(hashval_t))
#endif

void hash_init(void);
void hash_update(unsigned char *buffer, const int len);
void hash_complete(hashval_t *hp);

/* hash.h ends */

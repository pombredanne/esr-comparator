/* interface file for comparator hash function */

#ifdef CUSTOM_HASH
#define HASHSIZE	8
typedef unsigned long long	hashval_t;
#define hash_compare(s, t)	((s) - (t))
#else
#define HASHSIZE	16
typedef unsigned char	hashval_t[HASHSIZE];
#define hash_compare(s, t)	memcmp((s), (t), HASHSIZE)
#endif

void hash_init(void);
void hash_update(unsigned char *buffer, const int len);
void hash_complete(hashval_t *hp);

/* hash.h ends */

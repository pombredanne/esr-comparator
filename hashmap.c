/* hashmap.c -- custom data type for hash lists. */

#include <sys/types.h>
#include "shred.h"

void file_intern(const char *file)
{
    printf("Interning %s\n", file);
}

void hash_intern(const struct hash_t *chunk)
{
    printf("Chunk: %d, %d\n", chunk->start, chunk->end);
}

/* hashmap.c ends here */

#ifndef PAIR_H
#define PAIR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * pair is used as a thread-safe way of storing multiple name/value pairs. 
 * COMMENT: This is a map, not a "pair". The adt really needs to be renamed some day.
 */
struct pair_;
typedef struct pair_tag* pair;
pair	pair_new(size_t nelem);
void	pair_free(pair p);

int	pair_add(pair p, const char* name, const char* value);
int	pair_set(pair p, const char* name, const char* value);
const char* pair_get_name(pair p, size_t i);
const char* pair_get(pair p, const char* name);

/* Sometimes we iterate on index to get names. Then there is no
 * need to get the value by name as we have the index anyway.
 * We avoid a lot of strcmps() this way.
 */
const char* pair_get_value_by_index(pair p, size_t i);
size_t	pair_size(pair p);

#ifdef __cplusplus
}
#endif

#endif


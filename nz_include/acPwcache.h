#ifndef ACPWCACHE_INCLUDED
#define ACPWCACHE_INCLUDED

/*
 * "C" API for manipulating the password cache
 *
 * It is not intended for appcomps code to use this interface directly. Use
 * the C++ API in acPasswordCache.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* lookup the cached password for host and username. Caller must free the
 * result with free() */
bool pwcache_lookup_no_resolve(const char *host, const char *username, char **password);
bool pwcache_lookup(const char *host, const char *username, char **password);

/* remove cached password entry for username and host */
bool pwcache_delete(const char *host, const char *username);

/* remove all cached password entries */
bool pwcache_clear();

/* save password for host and username */
bool pwcache_save(const char *host, const char *username, const char *password);

/* get the current list of hosts/usernames in the cache and return number of
 * entries, or -1 on error */
int pwcache_enum(char ***hosts, char ***usernames);

/* free the arrays returned by pwcache_enum */
void pwcache_free_enum(char **hosts, char **usernames);

/* get the error message for the prior request */
const char *pwcache_errmsg();

bool pwcache_resetkey(bool none);

/* Control verbose output */
void pwcache_set_verbose(bool verbose);

/* Control Resolving hostnames */
void pwcache_set_resolve_mode(bool resolve);

#ifdef __cplusplus
}
#endif

#endif

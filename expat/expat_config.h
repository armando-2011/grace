#include <config.h>

#ifdef WORDS_BIGENDIAN
# define BYTEORDER 4321
#else
# define BYTEORDER 1234
#endif

/* Define to specify how much context to retain around the current parse
   point. */
#define XML_CONTEXT_BYTES 1024

/* Define to make parameter entity parsing functionality available. */
#define XML_DTD 1

/* Define to make XML Namespaces functionality available. */
#define XML_NS 1

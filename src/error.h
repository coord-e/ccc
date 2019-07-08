#ifndef CCC_ERROR_H
#define CCC_ERROR_H

void error(char *fmt, ...);

#define CCC_UNREACHABLE error("unreachable (%s:%d)", __FILE__, __LINE__)

#endif

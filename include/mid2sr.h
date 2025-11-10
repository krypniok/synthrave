#ifndef SYNTHRAVE_MID2SR_H
#define SYNTHRAVE_MID2SR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool mid2sr_convert(const char *mid_path, const char *aox_path);

#ifdef __cplusplus
}
#endif

#endif /* SYNTHRAVE_MID2SR_H */

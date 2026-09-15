#include "Application/force_build.h"
#include <math.h>
#include <string.h>
const ForceBuildConfig g_force_build_config={
#define BUILD_VALUE(n,v) .n=v,
 FORCE_BUILD_FIELDS(BUILD_VALUE)
#undef BUILD_VALUE
};
uint32_t ForceBuild_ConfigDigest(void)
{
 uint32_t h=2166136261U;
#define BUILD_HASH(n,v) for (unsigned i=0;i<4;i++) { h^=(g_force_build_config.n>>(8*i))&255U; h*=16777619U; }
 FORCE_BUILD_FIELDS(BUILD_HASH)
#undef BUILD_HASH
 return h;
}

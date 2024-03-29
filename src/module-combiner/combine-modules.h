#ifndef WABT_COMBINE_MODULES_H_
#define WABT_COMBINE_MODULES_H_

#include "wabt/common.h"

namespace wabt {

struct Module;

Result CombineModules( struct Module*, struct Module*, struct Module* );

}

#endif

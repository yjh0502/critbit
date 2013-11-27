#include <cstdio>
#include <cstdlib>
#include <google/sparse_hash_map>
#include <string>

#define MAP_TYPE google::sparse_hash_map<std::string, void *>

extern "C" {
#include "helper.h"
#include "cc_common.h"
}

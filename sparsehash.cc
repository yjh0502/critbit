
extern "C" {
#include "helper.h"
}

#include <cstdio>
#include <cstdlib>
#include <string>
#include <google/sparse_hash_map>

google::sparse_hash_map<std::string, int> m;

#include "cc_common.h"

#include "duckdb/storage/storage_info.hpp"

namespace duckdb {

const uint64_t VERSION_NUMBER = 65;

struct StorageVersionInfo {
	const char *version_name;
	idx_t storage_version;
};

// START OF STORAGE VERSION INFO
static const StorageVersionInfo storage_version_info[] = {
    {"v0.0.4", 1},   {"v0.1.0", 1},  {"v0.1.1", 1},  {"v0.1.2", 1},  {"v0.1.3", 1},   {"v0.1.4", 1},   {"v0.1.5", 1},
    {"v0.1.6", 1},   {"v0.1.7", 1},  {"v0.1.8", 1},  {"v0.1.9", 1},  {"v0.2.0", 1},   {"v0.2.1", 1},   {"v0.2.2", 4},
    {"v0.2.3", 6},   {"v0.2.4", 11}, {"v0.2.5", 13}, {"v0.2.6", 15}, {"v0.2.7", 17},  {"v0.2.8", 18},  {"v0.2.9", 21},
    {"v0.3.0", 25},  {"v0.3.1", 27}, {"v0.3.2", 31}, {"v0.3.3", 33}, {"v0.3.4", 33},  {"v0.3.5", 33},  {"v0.4.0", 33},
    {"v0.5.0", 38},  {"v0.5.1", 38}, {"v0.6.0", 39}, {"v0.6.1", 39}, {"v0.7.0", 43},  {"v0.7.1", 43},  {"v0.8.0", 51},
    {"v0.8.1", 51},  {"v0.9.0", 64}, {"v0.9.1", 64}, {"v0.9.2", 64}, {"v0.10.0", 64}, {"v0.10.1", 64}, {"v0.10.2", 64},
    {"v0.10.3", 65}, {nullptr, 0}};

const char *GetDuckDBVersion(idx_t version_number) {
	for (idx_t i = 0; storage_version_info[i].version_name; i++) {
		if (version_number == storage_version_info[i].storage_version) {
			return storage_version_info[i].version_name;
		}
	}
	return nullptr;
}

} // namespace duckdb

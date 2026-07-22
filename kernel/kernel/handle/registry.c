#include <kernel/handle/registry.h>
#include <string.h>

static registry_entry_t registry[MAX_REGISTERED];

int registry_add(const char* path, handle_type_t type, void* impl, const handle_ops_t* ops) {
	if (registry_find(path)) {
		return -1; // name already taken
	}
	for (int i = 0; i < MAX_REGISTERED; i++) {
		if (!registry[i].in_use) {
			strncpy(registry[i].path, path, MAX_PATH_LEN - 1);
			registry[i].path[MAX_PATH_LEN - 1] = '\0';
			registry[i].type = type;
			registry[i].impl = impl;
			registry[i].ops = ops;
			registry[i].in_use = true;
			return 0;
		}
	}
	return -1; // registry full
}

registry_entry_t* registry_find(const char* path) {
	for (int i = 0; i < MAX_REGISTERED; i++) {
		if (registry[i].in_use && strcmp(registry[i].path, path) == 0) {
			return &registry[i];
		}
	}
	return NULL;
}
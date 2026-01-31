#include "platform/platform.h"
#include "platform/posix.h"

#include "util.h"
#include "path.h"
#include "app.h"

err_t setup_cache(path_t *cache_path, arena_t *arena) {
	const char *home = getenv("HOME");
	if (home == NULL) return ERR;
	try(path_init(cache_path, arena, "", home, "Library/Caches/doc-cache") == ERR);
	return create_dir(cache_path);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s <input_dir>\n", argv[0]);
		return 1;
	}

	const char *input_dir = argv[1];
	app_main(input_dir);
}

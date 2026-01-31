#pragma once

#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "util.h"
#include "path.h"

// process

#define MAX_THREADS 16
#define MAX_CONFIG_LINE 1024

typedef struct Thread thread_t;
typedef void *(*thread_func_t)(void *);

err_t thread_create(arena_t *arena, thread_t **out_thread, thread_func_t func, void *arg);
err_t thread_join(thread_t *thread);
err_t thread_detach(thread_t *thread);

// file system

#define PATHLIST_DIRS_ONLY (1u << 0)
#define PATHLIST_RECURSIVE (1u << 1)
#define PATHLIST_FILES_ONLY (1u << 2)
err_t pathlist_init(pathlist_t *path_list, arena_t *arena, const char *full_path, unsigned flags);

bool path_exists(path_t *path);
err_t create_dir(path_t *path);
err_t clear_dir(path_t *dir_path, arena_t *scratch);
err_t remove_empty_dir(path_t *dir_path);
err_t copy_file(path_t *src_path, path_t *dst_path);

// cache

err_t setup_cache(path_t *cache_path, arena_t *arena);

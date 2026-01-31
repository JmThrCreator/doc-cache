#pragma once

#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

#include "util.h"
#include "platform.h"

extern char **environ;

// threads

struct Thread {
	pthread_t handle;
};

err_t thread_create(arena_t *arena, thread_t **out_thread, thread_func_t func, void *arg) {
	thread_t *thread = arena_push_struct(arena, thread_t);
	if (!thread) return ERR;

	if (pthread_create(&thread->handle, NULL, func, arg) != 0) return ERR;

	*out_thread = thread;
	return OK;
}

err_t thread_join(thread_t *thread) {
    if (!thread) return ERR;
    pthread_join(thread->handle, NULL);
    return OK;
}

err_t thread_detach(thread_t *thread) {
	if (!thread) return ERR;
	pthread_detach(thread->handle);
	return OK;
}

// file system

#define MAX_PATHS 100
#define PATH_SEPARATOR '/'

bool path_exists(path_t *path) {
	struct stat statbuf;
	return (stat(path->full_path, &statbuf) == 0);
}

err_t is_dir(const char *path) {
	struct stat statbuf;
	try(stat(path, &statbuf) != 0);
	return S_ISDIR(statbuf.st_mode);
}

err_t pathlist_collect(pathlist_t *path_list, arena_t *arena, const char *full_path, const char *relative_prefix, unsigned flags) {
	bool dirs_only = (flags & PATHLIST_DIRS_ONLY) != 0;
	bool recursive = (flags & PATHLIST_RECURSIVE) != 0;
	bool files_only = (flags & PATHLIST_FILES_ONLY) != 0;

	dev_assert(!(dirs_only && files_only), "DIRS_ONLY and FILES_ONLY both set");

	DIR *dir = opendir(full_path);
	if (!dir) return ERR;
	struct dirent *entry;

	for (; path_list->count < MAX_PATHS;) {
		if ((entry = readdir(dir)) == NULL) break;
		if (is_dir_reserved(entry->d_name)) continue;

		char child_full_path[MAX_PATH_LEN];
		int n = snprintf(child_full_path, MAX_PATH_LEN, "%s%c%s", full_path, PATH_SEPARATOR, entry->d_name);
		if (n < 0 || n >= MAX_PATH_LEN) {
			closedir(dir);
			return ERR;
		}

		bool add_entry = true;
		if (dirs_only && !is_dir(child_full_path)) add_entry = false;
		if (files_only && is_dir(child_full_path)) add_entry = false;

		char child_relative_prefix[MAX_PATH_LEN];
		if (relative_prefix[0] == '\0') {
			snprintf(child_relative_prefix, MAX_PATH_LEN, "%s", entry->d_name);
		} else {
			snprintf(child_relative_prefix, MAX_PATH_LEN, "%s%c%s", relative_prefix, PATH_SEPARATOR, entry->d_name);
		}
		
		if (add_entry) {
			path_t *child_path = arena_push_struct(arena, path_t);
			if (path_init(child_path, arena, relative_prefix, child_full_path) == ERR) {
				closedir(dir);
				return ERR;
			}

			path_list->items[path_list->count++] = child_path;
		}
		
		if (recursive && is_dir(child_full_path)) {
			if (pathlist_collect(path_list, arena, child_full_path, child_relative_prefix, flags) == ERR) {
				return ERR;
			}
		}
	}
	closedir(dir);

	return OK;
}

err_t pathlist_init(pathlist_t *path_list, arena_t *arena, const char *full_path, unsigned flags) {
	path_list->count = 0;
	path_list->capacity = MAX_PATHS;

	// root path
	path_list->path = arena_push_struct(arena, path_t);
	try(path_init(path_list->path, arena, "", full_path));

	// storage
	path_list->items = arena_push_array(arena, MAX_PATHS, path_t*);
	if (!path_list->items) return ERR;

	return pathlist_collect(path_list, arena, full_path, "", flags);
}

err_t create_dir(path_t *path) {
	if (path_exists(path)) return OK;
	//dev_assert(path->is_dir, "create_dir called on a file path");
	return mkdir(path->full_path, 0755);
}

err_t clear_dir(path_t *dir_path, arena_t *scratch) {
	DIR *dp = opendir(dir_path->full_path);
	if (!dp) return ERR;

	struct dirent *entry;
	while ((entry = readdir(dp)) != NULL) {
		if (is_dir_reserved(entry->d_name)) continue;
		
		path_t path;
		if (path_init(&path, scratch, "", dir_path->full_path, entry->d_name) == ERR) {
        		closedir(dp);
        		return ERR;
        	}

		if (is_dir(path.full_path)) {
			if (clear_dir(&path, scratch) == ERR) {
                		closedir(dp);
                		return ERR;
			}
			if (rmdir(path.full_path) != 0) {
				closedir(dp);
				return ERR;
			}
		} else {
			if (unlink(path.full_path) != 0) {
				closedir(dp);
				return ERR;
			}
		}
	}
	closedir(dp);
	return OK;
}

err_t remove_empty_dir(path_t *dir_path) {
	return rmdir(dir_path->full_path);
}

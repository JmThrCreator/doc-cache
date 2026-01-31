#pragma once

#include "util.h"
#include "cache.h"
#include "platform/platform.h"
#include <stdio.h>

//#define INPUT_DIR "/Users/jmaggio/Documents/Code/doc-viewer/example_input"

#define THUMBNAIL_FOLDER_NAME "thumbnails"
#define THREAD_ARENA_SIZE 32*1024

static const char *ALLOWED_EXTS[] = {"pdf", "png", "jpg", "jpeg"};
#define ALLOWED_EXTS_COUNT (sizeof(ALLOWED_EXTS) / sizeof(ALLOWED_EXTS[0]))

typedef struct CacheBuilderOptions {
    const char *input_dir;
    const char *output_cache_dir;
} CacheBuilderOptions;

/*
each PDF and image file is a folder in the cache
these are refered to as "doc folders"
the images and PDF pages (converted to images) are stored inside doc folders
*/

void app_main(const char* input_dir) {
	arena_t scratch;
	arena_init(&scratch, 10*1000*1024);
	
	path_t cache_path;
	if (setup_cache(&cache_path, &scratch) == ERR) {
		printf("Cache could not be created");
		return;
	}

	clear_dir(&cache_path, &scratch); // be careful!

	// LAYER 1: create all sub-folders, including doc folders
	
	pathlist_t sub_folders;
	pathlist_init(&sub_folders, &scratch, input_dir, PATHLIST_RECURSIVE);
	pathlist_filter_by_ext(&sub_folders, ALLOWED_EXTS, ALLOWED_EXTS_COUNT);

	for (int j = 0; j < sub_folders.count; j++) {
		path_t sub_folder;
		path_init(
			&sub_folder, &scratch, "", cache_path.full_path,
			sub_folders.items[j]->relative_prefix, sub_folders.items[j]->name);
		create_dir(&sub_folder);
	}

	// LAYER 2: render thumbnails

	pathlist_t real_sub_folders; // exclude doc folders
	pathlist_init(&real_sub_folders, &scratch, input_dir, PATHLIST_RECURSIVE | PATHLIST_DIRS_ONLY);

	path_t top_thumbnail_folder;
	path_init(&top_thumbnail_folder, &scratch, "", cache_path.full_path, THUMBNAIL_FOLDER_NAME);
	create_dir(&top_thumbnail_folder);

	for (int j = 0; j < real_sub_folders.count; j++) {
		path_t thumbnail_folder;
		path_init(&thumbnail_folder, &scratch, "", cache_path.full_path,
			real_sub_folders.items[j]->relative_prefix, real_sub_folders.items[j]->name, THUMBNAIL_FOLDER_NAME);
		create_dir(&thumbnail_folder);
	}

	pathlist_t files;
	pathlist_init(&files, &scratch, input_dir, PATHLIST_RECURSIVE | PATHLIST_FILES_ONLY);
	pathlist_filter_by_ext(&files, ALLOWED_EXTS, ALLOWED_EXTS_COUNT);
	
	thread_t *threads[MAX_THREADS];
	int threads_running = 0;
	for (int j = 0; j < files.count; j++) {
		path_t *file_path = files.items[j];

		char thumbnail_out_name[MAX_PATH_LEN];
		if (snprintf(thumbnail_out_name, sizeof(thumbnail_out_name), "%s.jpeg", file_path->stem) >= sizeof(thumbnail_out_name)) continue;
	
		path_t *thumbnail_out = arena_push_struct(&scratch, path_t);
		path_init(thumbnail_out, &scratch, "", cache_path.full_path,
			file_path->relative_prefix, THUMBNAIL_FOLDER_NAME, thumbnail_out_name);
		
		create_thumbnail_task_t *task = arena_push_struct(&scratch, create_thumbnail_task_t);
		task->file_path = file_path;
		task->out_path = thumbnail_out;

		thread_create(&scratch, &threads[threads_running], create_thumbnail, task);
		threads_running++;

		if (threads_running == MAX_THREADS) {
			for (int k = 0; k < threads_running; k++) {
				thread_join(threads[k]);
			}
			threads_running = 0;
		}
	}
	for (int k = 0; k < threads_running; k++) {
		thread_join(threads[k]);
	}

	// LAYER 3: create full render
	
	// each thread has its own arena space, which it reuses for every task
	arena_t thread_arenas[MAX_THREADS];
	uint8_t *block = arena_push_array(&scratch, MAX_THREADS * THREAD_ARENA_SIZE, uint8_t);
	for (int i = 0; i < MAX_THREADS; i++) {
		thread_arenas[i].base = block + i * THREAD_ARENA_SIZE;
    		thread_arenas[i].capacity = THREAD_ARENA_SIZE;
    		thread_arenas[i].offset = 0;
	}
	
	threads_running = 0;
	for (int j = 0; j < files.count; j++) {
		path_t *file_path = files.items[j];
	
		path_t *out_dir = arena_push_struct(&scratch, path_t);
		path_init(out_dir, &scratch, "", cache_path.full_path,
			file_path->relative_prefix, file_path->name);
	
		int thread_index = threads_running;
		arena_t *thread_arena = &thread_arenas[thread_index];
		
		create_full_render_task_t *task = arena_push_struct(&scratch, create_full_render_task_t);
		task->arena = thread_arena;
		task->file_path = file_path;
		task->out_dir = out_dir;

		thread_create(&scratch, &threads[threads_running], create_full_render, task);
		threads_running++;

		if (threads_running == MAX_THREADS) {
			for (int k = 0; k < threads_running; k++) {
				thread_join(threads[k]);
				thread_arenas[k].offset = 0;
			}
			threads_running = 0;
		}
	}
	for (int k = 0; k < threads_running; k++) {
		thread_join(threads[k]);
	}

	//printf("%.1f/%.1f MB\n", (double)scratch.offset / 1000.0 / 1024.0, (double)scratch.capacity / 1000.0 / 1024.0);
	return;
}

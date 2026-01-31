#pragma once

#include "util.h"
#include "platform/platform.h"

#include "fitz/document.h"
#include "fitz/write-pixmap.h"
#include "fitz/util.h"

#define THUMBNAIL_JPEG_QUALITY 90

typedef struct CreateFullRenderTask {
	arena_t *arena;
	path_t *file_path;
	path_t *out_dir;
} create_full_render_task_t;

typedef struct CreateThumbnailTask {
	path_t *file_path;
	path_t *out_path;
} create_thumbnail_task_t;

typedef struct PdfInfo {
	fz_context *ctx;
	fz_document *doc;
	int page_count;
} pdf_info_t;

err_t copy_file(path_t *src_path, path_t *dst_path) {
	FILE *src_file = fopen(src_path->full_path, "rb");
	if (!src_file) return ERR;

	FILE *dst_file = fopen(dst_path->full_path, "wb");
	if (dst_file == NULL) {
		fclose(src_file);
		return ERR;
	}

	char buf[8192];
	size_t bytes_read;
	while ((bytes_read = fread(buf, 1, sizeof(buf), src_file)) > 0) {
		if (fwrite(buf, 1, bytes_read, dst_file) != bytes_read) {
			fclose(src_file);
			fclose(dst_file);
			return ERR;
		}
	}
	if (ferror(src_file)) {
		fclose(src_file);
		fclose(dst_file);
		return ERR;
	}

	fclose(src_file);
	fclose(dst_file);
	return OK;
}

err_t png_to_jpeg(path_t *src_path, path_t *dst_path) {
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx) return false;

	fz_image *image = NULL;
	fz_pixmap *pixmap = NULL;

	fz_try(ctx) {
		fz_open_file(ctx, src_path->full_path);
		image = fz_new_image_from_file(ctx, src_path->full_path);
        	pixmap = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, 0);

        	fz_save_pixmap_as_jpeg(ctx, pixmap, dst_path->full_path, THUMBNAIL_JPEG_QUALITY);
	}
	fz_always(ctx) {
        	if (pixmap) fz_drop_pixmap(ctx, pixmap);
		if (image) fz_drop_image(ctx, image);
        	fz_drop_context(ctx);
	}
	fz_catch(ctx) {
		return ERR;
	}
	return OK;
}

err_t jpeg_to_png(path_t *src_path, path_t *dst_path) {
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx) return ERR;

	fz_image *image = NULL;
	fz_pixmap *pixmap = NULL;

	fz_try(ctx) {
		fz_open_file(ctx, src_path->full_path);
		image = fz_new_image_from_file(ctx, src_path->full_path);
        	pixmap = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, 0);

        	fz_save_pixmap_as_png(ctx, pixmap, dst_path->full_path);
    	}
    	fz_always(ctx) {
        	if (pixmap) fz_drop_pixmap(ctx, pixmap);
		if (image) fz_drop_image(ctx, image);
        	fz_drop_context(ctx);
	}
	fz_catch(ctx) {
		return ERR;
	}
    return OK;
}

static void ignore_mupdf_msg(void *user, const char *message) { (void)user; (void)message; } // stop mupdf warnings

err_t pdf_info_init(pdf_info_t *pdf_info, path_t *pdf_path) {
	fz_context *ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx) return ERR;
	fz_set_warning_callback(ctx, ignore_mupdf_msg, NULL);
	fz_set_error_callback(ctx, ignore_mupdf_msg, NULL);

	fz_document *doc;
	int page_count = 1;
	fz_try(ctx) {
		fz_register_document_handlers(ctx);
		doc = fz_open_document(ctx, pdf_path->full_path);
		page_count = fz_count_pages(ctx, doc);
	}
	fz_catch(ctx) {
		fz_drop_context(ctx);
		return ERR;
	}

	pdf_info->ctx = ctx;
	pdf_info->doc = doc;
	pdf_info->page_count = page_count;
	return OK;
}

void pdf_info_free(pdf_info_t *pdf_info) {
	fz_drop_document(pdf_info->ctx, pdf_info->doc);
	fz_drop_context(pdf_info->ctx);
}

err_t convert_pdf_thumbnail(pdf_info_t *pdf_info, path_t *dst_path) {
	fz_pixmap *pixmap;
	fz_try(pdf_info->ctx) {
		fz_matrix matrix = fz_identity;
		pixmap = fz_new_pixmap_from_page_number(pdf_info->ctx, pdf_info->doc, 0, matrix, fz_device_rgb(pdf_info->ctx), 0);
		fz_save_pixmap_as_jpeg(pdf_info->ctx, pixmap, dst_path->full_path, THUMBNAIL_JPEG_QUALITY);
		fz_drop_pixmap(pdf_info->ctx, pixmap);
		pixmap = NULL;
	}
	fz_catch(pdf_info->ctx) {
		if (pixmap) fz_drop_pixmap(pdf_info->ctx, pixmap);
		return ERR;
	}
	return OK;
}

err_t convert_pdf_pages(pdf_info_t *pdf_info, path_t *out_dir, arena_t *scratch) {
	fz_pixmap *pixmap;
	float dpi = 300.0f;
	float scale = dpi / 72.0f;

	fz_try(pdf_info->ctx) {
		fz_matrix matrix = fz_scale(scale, scale);
		for (int i = 0; i < pdf_info->page_count; i++) {
			char file_name[MAX_PATH_LEN];
			if (snprintf(file_name, sizeof(file_name), "page-%i.png", i) >= sizeof(file_name)) return false;
			path_t page_path;
			if (path_init(&page_path, scratch, "", out_dir->full_path, file_name) == ERR) return false;

			pixmap = fz_new_pixmap_from_page_number(pdf_info->ctx, pdf_info->doc, i, matrix, fz_device_rgb(pdf_info->ctx), 0);
			fz_save_pixmap_as_png(pdf_info->ctx, pixmap, page_path.full_path);
			fz_drop_pixmap(pdf_info->ctx, pixmap);
			pixmap = NULL;
		}
	}
	fz_catch(pdf_info->ctx) {
		if (pixmap) fz_drop_pixmap(pdf_info->ctx, pixmap);
		return ERR;
	}
	return OK;
}

void *create_thumbnail(void *arg) {
	create_thumbnail_task_t *task = (create_thumbnail_task_t *)arg;
	path_t *file_path = task->file_path;
	path_t *out_path = task->out_path;

	if (strncmp(file_path->ext, "pdf", MAX_PATH_LEN) == 0) {
		pdf_info_t pdf_info = {0};
		if (pdf_info_init(&pdf_info, file_path) == ERR) return NULL;
		if (convert_pdf_thumbnail(&pdf_info, out_path) == ERR) return NULL;
		pdf_info_free(&pdf_info);
	}
	else if (strncmp(file_path->ext, "png", MAX_PATH_LEN) == 0) {
		png_to_jpeg(file_path, out_path);
	}
	else if (strstr("jpg, jpeg", file_path->ext)) {
		copy_file(file_path, out_path);
	}
	return NULL;
}

void *create_full_render(void *arg) {
	create_full_render_task_t *task = (create_full_render_task_t *)arg;
	arena_t *arena = task->arena;
	path_t *file_path = task->file_path;
	path_t *out_dir = task->out_dir;

	if (strncmp(file_path->ext, "pdf", MAX_PATH_LEN) == 0) {
		pdf_info_t pdf_info = {0};
		if (pdf_info_init(&pdf_info, file_path) == ERR) return NULL;
		if (convert_pdf_pages(&pdf_info, out_dir, arena) == ERR) return NULL;
		pdf_info_free(&pdf_info);
	}
	else {
		path_t out_path;
		if (path_init(&out_path, arena, "", out_dir->full_path, "0.png") == ERR) return NULL;
		if (strstr("jpg,jpeg", file_path->ext)) {
			if (jpeg_to_png(file_path, &out_path) == ERR) return NULL;
		} else {
			if (copy_file(file_path, &out_path) == ERR) return NULL;
		}
	}

	return NULL;
}

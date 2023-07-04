/////////////////////////////////////////////
//                                         //
//    Copyright (C) 2019-2019 Julian Uy    //
//  https://sites.google.com/site/awertyb  //
//                                         //
//   See details of license at "LICENSE"   //
//                                         //
/////////////////////////////////////////////

#include "extractor.h"
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const char *plugin_info[4] = {
    "00IN",
    "PNG Plugin for Susie Image Viewer",
    "*.png",
    "PNG file (*.png)",
};

const int header_size = 64;

typedef struct {
	const uint8_t *buf;
	size_t size;
	size_t cur;
} data_pointer;

static void PNG_read_data(png_structp png_ptr, png_bytep data,
                          png_size_t length) {
	data_pointer *d = ((data_pointer *)png_get_io_ptr(png_ptr));
	if (d->cur + length > d->size)
		length = d->size - d->cur;

	memcpy(data, d->buf + d->cur, length);
	d->cur += length;
}

int getBMPFromPNG(const uint8_t *input_data, size_t file_size,
                  BITMAPFILEHEADER *bitmap_file_header,
                  BITMAPINFOHEADER *bitmap_info_header, uint8_t **data) {
	png_structp png_ptr =
	    png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return -1;
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		return -1;
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
		return -1;
	if (setjmp(png_jmpbuf(png_ptr))){
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return -1;
	}

	data_pointer d;
	d.buf = input_data;
	d.size = file_size;
	d.cur = 0;

	png_set_read_fn(png_ptr, (png_voidp)&d, (png_rw_ptr)PNG_read_data);
	png_set_sig_bytes(png_ptr, 0);

	png_read_info(png_ptr, info_ptr);
	int width = png_get_image_width(png_ptr, info_ptr);
	int height = png_get_image_height(png_ptr, info_ptr);
	png_byte color_type = png_get_color_type(png_ptr, info_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
			png_set_palette_to_rgb(png_ptr);
			png_set_tRNS_to_alpha(png_ptr);
			color_type = PNG_COLOR_TYPE_RGB_ALPHA;
		} else {
			png_set_palette_to_rgb(png_ptr);
			color_type = PNG_COLOR_TYPE_RGB;
		}
	}

	switch (color_type) {
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		png_set_gray_to_rgb(png_ptr);
		color_type = PNG_COLOR_TYPE_RGB_ALPHA;
		png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
		break;
	case PNG_COLOR_TYPE_GRAY:
		png_set_expand(png_ptr);
		png_set_gray_to_rgb(png_ptr);
		color_type = PNG_COLOR_TYPE_RGB;
		png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		png_set_bgr(png_ptr);
		break;
	case PNG_COLOR_TYPE_RGB:
		png_set_bgr(png_ptr);
		png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
		break;
	default:
		return -1;
	}

	png_read_update_info(png_ptr, info_ptr);

	int bit_width = width * 4;
	int bit_length = bit_width;

	uint8_t* bitmap_data =
		(uint8_t*)malloc(sizeof(uint8_t) * bit_length * height);
	if (!bitmap_data) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return -1;
	}
	png_bytepp row_pointers = (png_bytepp)malloc(sizeof(png_bytep) * height);
	if (!row_pointers) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return -1;
	}

	for (int y = 0; y < height; y++) {
		row_pointers[y] =
			(png_bytep)(bitmap_data + (height - y - 1) * bit_length);
	}

	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, info_ptr);

	free(row_pointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	*data = bitmap_data;

	memset(bitmap_file_header, 0, sizeof(BITMAPFILEHEADER));
	memset(bitmap_info_header, 0, sizeof(BITMAPINFOHEADER));

	bitmap_file_header->bfType = 'M' * 256 + 'B';
	bitmap_file_header->bfSize = sizeof(BITMAPFILEHEADER) +
	                             sizeof(BITMAPINFOHEADER) +
	                             sizeof(uint8_t) * bit_length * height;
	bitmap_file_header->bfOffBits =
	    sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bitmap_file_header->bfReserved1 = 0;
	bitmap_file_header->bfReserved2 = 0;

	bitmap_info_header->biSize = 40;
	bitmap_info_header->biWidth = width;
	bitmap_info_header->biHeight = height;
	bitmap_info_header->biPlanes = 1;
	bitmap_info_header->biBitCount = 32;
	bitmap_info_header->biCompression = 0;
	bitmap_info_header->biSizeImage = bitmap_file_header->bfSize;
	bitmap_info_header->biXPelsPerMeter = bitmap_info_header->biYPelsPerMeter =
	    0;
	bitmap_info_header->biClrUsed = 0;
	bitmap_info_header->biClrImportant = 0;
	return 0;
}

BOOL IsSupportedEx(const char *data) {
	const char header[] = {0x89, 'P', 'N', 'G'};
	for (int i = 0; i < sizeof(header); i++) {
		if (header[i] == 0x00)
			continue;
		if (data[i] != header[i])
			return FALSE;
	}
	return TRUE;
}

int GetPictureInfoEx(size_t data_size, const char *data,
                     SusiePictureInfo *picture_info) {
	png_structp png_ptr =
	    png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return SPI_MEMORY_ERROR;
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		return SPI_MEMORY_ERROR;
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
		return SPI_MEMORY_ERROR;
	if (setjmp(png_jmpbuf(png_ptr)))
		return SPI_MEMORY_ERROR;

	data_pointer d;
	d.buf = (uint8_t *)data;
	d.size = data_size;
	d.cur = 0;

	png_set_read_fn(png_ptr, (png_voidp)&d, (png_rw_ptr)PNG_read_data);
	png_set_sig_bytes(png_ptr, 0);

	png_read_info(png_ptr, info_ptr);

	int width = png_get_image_width(png_ptr, info_ptr);
	int height = png_get_image_height(png_ptr, info_ptr);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	picture_info->left = 0;
	picture_info->top = 0;
	picture_info->width = width;
	picture_info->height = height;
	picture_info->x_density = 0;
	picture_info->y_density = 0;
	picture_info->colorDepth = 32;
	picture_info->hInfo = NULL;

	return SPI_ALL_RIGHT;
}

int GetPictureEx(size_t data_size, HANDLE *bitmap_info, HANDLE *bitmap_data,
                 SPI_PROGRESS progress_callback, intptr_t user_data, const char *data) {
	uint8_t *data_u8;
	BITMAPINFOHEADER bitmap_info_header;
	BITMAPFILEHEADER bitmap_file_header;
	BITMAPINFO *bitmap_info_locked;
	unsigned char *bitmap_data_locked;

	if (progress_callback != NULL)
		if (progress_callback(1, 1, user_data))
			return SPI_ABORT;

	if (getBMPFromPNG((const uint8_t *)data, data_size, &bitmap_file_header,
	                   &bitmap_info_header, &data_u8))
		return SPI_MEMORY_ERROR;
	*bitmap_info = LocalAlloc(LMEM_MOVEABLE | LMEM_ZEROINIT, sizeof(BITMAPINFO));
	*bitmap_data = LocalAlloc(LMEM_MOVEABLE, bitmap_file_header.bfSize -
	                                             bitmap_file_header.bfOffBits);
	if (*bitmap_info == NULL || *bitmap_data == NULL) {
		if (*bitmap_info != NULL)
			LocalFree(*bitmap_info);
		if (*bitmap_data != NULL)
			LocalFree(*bitmap_data);
		return SPI_NO_MEMORY;
	}
	bitmap_info_locked = (BITMAPINFO *)LocalLock(*bitmap_info);
	bitmap_data_locked = (unsigned char *)LocalLock(*bitmap_data);
	if (bitmap_info_locked == NULL || bitmap_data_locked == NULL) {
		LocalFree(*bitmap_info);
		LocalFree(*bitmap_data);
		return SPI_MEMORY_ERROR;
	}
	bitmap_info_locked->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info_locked->bmiHeader.biWidth = bitmap_info_header.biWidth;
	bitmap_info_locked->bmiHeader.biHeight = bitmap_info_header.biHeight;
	bitmap_info_locked->bmiHeader.biPlanes = 1;
	bitmap_info_locked->bmiHeader.biBitCount = 32;
	bitmap_info_locked->bmiHeader.biCompression = BI_RGB;
	bitmap_info_locked->bmiHeader.biSizeImage = 0;
	bitmap_info_locked->bmiHeader.biXPelsPerMeter = 0;
	bitmap_info_locked->bmiHeader.biYPelsPerMeter = 0;
	bitmap_info_locked->bmiHeader.biClrUsed = 0;
	bitmap_info_locked->bmiHeader.biClrImportant = 0;
	memcpy(bitmap_data_locked, data_u8,
	       bitmap_file_header.bfSize - bitmap_file_header.bfOffBits);

	LocalUnlock(*bitmap_info);
	LocalUnlock(*bitmap_data);
	free(data_u8);

	if (progress_callback != NULL)
		if (progress_callback(1, 1, user_data))
			return SPI_ABORT;

	return SPI_ALL_RIGHT;
}

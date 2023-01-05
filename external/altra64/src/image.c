
#include <libdragon.h>
#include "types.h"
#include "image.h"

//#define STBI_HEADER_FILE_ONLY
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


/*
 * Load an image from the rom filesystem, returning a pointer to the
 * sprite that hold the image.
 */
sprite_t *loadImageDFS(char *fname) {
    int size, x, y, n, fd;
    u8 *tbuf;
    u8 *ibuf;
    sprite_t *sbuf;

    fd = dfs_open(fname);
    if (fd < 0)
        return 0;                       // couldn't open image

    size = dfs_size(fd);
    tbuf = malloc(size);
    if (!tbuf) {
        dfs_close(fd);
        return 0;                       // out of memory
    }

    dfs_read(tbuf, 1, size, fd);
    dfs_close(fd);

    ibuf = stbi_load_from_memory(tbuf, size, &x, &y, &n, 4);
    free(tbuf);
    if (!ibuf)
        return 0;                       // couldn't decode image

    sbuf = (sprite_t*)malloc(sizeof(sprite_t) + x * y * 2);
    if (!sbuf) {
        stbi_image_free(ibuf);
        return 0;                       // out of memory
    }
    sbuf->width = x;
    sbuf->height = y;
    sbuf->bitdepth = 2;
    sbuf->format = 0;
    sbuf->hslices = x / 32;
    sbuf->vslices = y / 16;

    color_t *src = (color_t*)ibuf;
    u16 *dst = (u16*)((u32)sbuf + sizeof(sprite_t));

    for (int j=0; j<y; j++)
        for (int i=0; i<x; i++)
            dst[i + j*x] = graphics_convert_color(src[i + j*x]) & 0x0000FFFF;

    /* Invalidate data associated with sprite in cache */
    data_cache_hit_writeback_invalidate( sbuf->data, sbuf->width * sbuf->height * sbuf->bitdepth );

    stbi_image_free(ibuf);
    return sbuf;
}


sprite_t *loadImage32(u8 *png, int size) {
    int x, y, n, fd;
    u8 *tbuf;
    u32 *ibuf;
    sprite_t *sbuf;

    tbuf = malloc(size);
    memcpy(tbuf,png,size);

    ibuf = (u32*)stbi_load_from_memory(tbuf, size, &x, &y, &n, 4);
    free(tbuf);
    if (!ibuf)
        return 0;                       // couldn't decode image

    sbuf = (sprite_t*)malloc(sizeof(sprite_t) + x * y * 4);
    if (!sbuf) {
        stbi_image_free(ibuf);
        return 0;                       // out of memory
    }

    sbuf->width = x;
    sbuf->height = y;
    sbuf->bitdepth = 4;
    sbuf->format = 0;
    sbuf->hslices = x / 32;
    sbuf->vslices = y / 32;

   // color_t *src = (color_t*)ibuf;
    u32 *dst = (u32*)((u32)sbuf + sizeof(sprite_t));

    for (int j=0; j<y; j++)
        for (int i=0; i<x; i++)
            dst[i + j*x] = ibuf[i + j*x];

    /* Invalidate data associated with sprite in cache */
    data_cache_hit_writeback_invalidate( sbuf->data, sbuf->width * sbuf->height * sbuf->bitdepth );

    stbi_image_free(ibuf);
    return sbuf;
}

sprite_t *loadImage32DFS(char *fname) {
    int size, x, y, n, fd;
    u8 *tbuf;
    u32 *ibuf;
    sprite_t *sbuf;

    fd = dfs_open(fname);
    if (fd < 0)
        return 0;                       // couldn't open image

    size = dfs_size(fd);
    tbuf = malloc(size);
    if (!tbuf) {
        dfs_close(fd);
        return 0;                       // out of memory
    }

    dfs_read(tbuf, 1, size, fd);
    dfs_close(fd);

    ibuf = (u32*)stbi_load_from_memory(tbuf, size, &x, &y, &n, 4);
    free(tbuf);
    if (!ibuf)
        return 0;                       // couldn't decode image

    sbuf = (sprite_t*)malloc(sizeof(sprite_t) + x * y * 4);
    if (!sbuf) {
        stbi_image_free(ibuf);
        return 0;                       // out of memory
    }

    sbuf->width = x;
    sbuf->height = y;
    sbuf->bitdepth = 4;
    sbuf->format = 0;
    sbuf->hslices = x / 32;
    sbuf->vslices = y / 32;

   // color_t *src = (color_t*)ibuf;
    u32 *dst = (u32*)((u32)sbuf + sizeof(sprite_t));

    for (int j=0; j<y; j++)
        for (int i=0; i<x; i++)
            dst[i + j*x] = ibuf[i + j*x];

    /* Invalidate data associated with sprite in cache */
    data_cache_hit_writeback_invalidate( sbuf->data, sbuf->width * sbuf->height * sbuf->bitdepth );

    stbi_image_free(ibuf);
    return sbuf;
}

/*
 * Draw an image to the screen using the sprite passed.
 */
void drawImage(display_context_t dcon, sprite_t *sprite) {
    int x, y = 0;

    rdp_sync(SYNC_PIPE);
    rdp_set_default_clipping();
    rdp_enable_texture_copy();
    //rdp_attach_display(dcon);
    rdp_attach(dcon);
    // Draw image
    for (int j=0; j<sprite->vslices; j++) {
        x = 0;
        for (int i=0; i<sprite->hslices; i++) {
            rdp_sync(SYNC_PIPE);
            rdp_load_texture_stride(0, 0, MIRROR_DISABLED, sprite, j*sprite->hslices + i);
            rdp_draw_sprite(0, x, y, MIRROR_DISABLED);
            x += 32;
        }
        y += 16;
    }
    //rdp_detach_display();
}
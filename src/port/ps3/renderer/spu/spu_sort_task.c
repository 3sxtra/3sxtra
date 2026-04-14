#include <spu_mfcio.h>
#include <spu_intrinsics.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    float z;
    uint32_t index;
} SPU_SortItem __attribute__((aligned(8)));

#define MAX_ITEMS 8192

typedef struct {
    uint64_t ea_items;    /* Changed to 64-bit for RPCS3 EAH compliance */
    uint32_t item_count;
    uint32_t run_command;
    uint32_t status;
    uint32_t pad[2];
} SortContext __attribute__((aligned(128)));

static SPU_SortItem items[MAX_ITEMS] __attribute__((aligned(128)));
static SPU_SortItem merge_temp[MAX_ITEMS] __attribute__((aligned(128)));

/* Helper to ensure EAH is zeroed/handled for RPCS3 */
static inline void dma_get(void* ls, uint64_t ea, uint32_t size, uint32_t tag) {
    mfc_get(ls, ea, size, tag, 0, 0);
}

static inline void dma_put(void* ls, uint64_t ea, uint32_t size, uint32_t tag) {
    mfc_put(ls, ea, size, tag, 0, 0);
}

int main(unsigned long long arg1, unsigned long long arg2, unsigned long long arg3, unsigned long long arg4) {
    SortContext ctx __attribute__((aligned(128)));
    uint64_t ctx_ea = (uint64_t)arg1;

    /* Signal ready */
    ctx.status = 1;
    dma_put(&ctx.status, ctx_ea + offsetof(SortContext, status), 4, 31);
    mfc_write_tag_mask(1 << 31);
    mfc_read_tag_status_all();

    while (1) {
        /* Wait for command */
        do {
            dma_get(&ctx, ctx_ea, sizeof(SortContext), 31);
            mfc_write_tag_mask(1 << 31);
            mfc_read_tag_status_all();
        } while (ctx.run_command == 0);

        if (ctx.run_command == 2)
            break;

        uint32_t n = ctx.item_count;
        if (n > MAX_ITEMS) n = MAX_ITEMS;

        uint32_t bytes_to_read = n * sizeof(SPU_SortItem);
        uint32_t xfer_size = (bytes_to_read + 15) & ~15;

        if (xfer_size > 0 && ctx.ea_items != 0) {
            /* DMA GET data */
            for (uint32_t offset = 0; offset < xfer_size; offset += 16384) {
                uint32_t chunk = xfer_size - offset;
                if (chunk > 16384) chunk = 16384;
                dma_get((uint8_t*)items + offset, ctx.ea_items + offset, chunk, 31);
            }
            mfc_write_tag_mask(1 << 31);
            mfc_read_tag_status_all();

            if (n > 1) {
                SPU_SortItem* src = items;
                SPU_SortItem* dst = merge_temp;
                
                for (int width = 1; width < n; width *= 2) {
                    for (int left = 0; left < n; left += 2 * width) {
                        int mid = left + width;
                        int right = left + 2 * width;
                        
                        if (mid >= n) {
                            for (int i = left; i < n; i++) dst[i] = src[i];
                            continue;
                        }
                        if (right > n) right = n;

                        /* Scalar merge with SIMD-style thinking/branch reduction */
                        int i = left, j = mid, k = left;
                        while (i < mid && j < right) {
                            /* Branchless selection of the smaller Z value */
                            int take_i = (src[i].z <= src[j].z);
                            if (take_i) {
                                dst[k++] = src[i++];
                            } else {
                                dst[k++] = src[j++];
                            }
                        }
                        while (i < mid) dst[k++] = src[i++];
                        while (j < right) dst[k++] = src[j++];
                    }
                    SPU_SortItem* tmp = src;
                    src = dst;
                    dst = tmp;
                }

                if (src != items) {
                    /* Final copy using quadword moves */
                    vector unsigned int* vsrc = (vector unsigned int*)src;
                    vector unsigned int* vitems = (vector unsigned int*)items;
                    uint32_t vcount = (n * sizeof(SPU_SortItem) + 15) / 16;
                    for (uint32_t i = 0; i < vcount; i++) {
                        vitems[i] = vsrc[i];
                    }
                }
            }

            /* DMA PUT data */
            for (uint32_t offset = 0; offset < xfer_size; offset += 16384) {
                uint32_t chunk = xfer_size - offset;
                if (chunk > 16384) chunk = 16384;
                dma_put((uint8_t*)items + offset, ctx.ea_items + offset, chunk, 31);
            }
            mfc_write_tag_mask(1 << 31);
            mfc_read_tag_status_all();
        }

        /* Signal completion */
        ctx.run_command = 0;
        ctx.status = 0;
        dma_put(&ctx, ctx_ea, sizeof(SortContext), 31);
        mfc_write_tag_mask(1 << 31);
        mfc_read_tag_status_all();
    }

    return 0;
}


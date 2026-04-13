#include <spu_mfcio.h>
#include <stdint.h>

typedef struct {
    float z;
    uint32_t index;
} SPU_SortItem __attribute__((aligned(8)));

#define MAX_ITEMS 8192

typedef struct {
    uint32_t ea_items;
    uint32_t item_count;
    uint32_t run_command;
    uint32_t status;
} SortContext __attribute__((aligned(16)));

static SPU_SortItem items[MAX_ITEMS] __attribute__((aligned(128)));
static SPU_SortItem merge_temp[MAX_ITEMS] __attribute__((aligned(128)));

int main(unsigned long long arg1, unsigned long long arg2, unsigned long long arg3, unsigned long long arg4) {
    SortContext ctx __attribute__((aligned(128)));

    uint32_t ctx_ea = (uint32_t)arg1;

    ctx.status = 1;
    mfc_put(&ctx.status, ctx_ea + 12, 4, 31, 0, 0);
    mfc_write_tag_mask(1 << 31);
    mfc_read_tag_status_all();

    while (1) {
        do {
            mfc_get(&ctx, ctx_ea, sizeof(SortContext), 31, 0, 0);
            mfc_write_tag_mask(1 << 31);
            mfc_read_tag_status_all();
        } while (ctx.run_command == 0);

        if (ctx.run_command == 2)
            break;

        uint32_t n = ctx.item_count;
        if (n > MAX_ITEMS)
            n = MAX_ITEMS;

        // Audit Fix:
        // 1. Calculate the exact byte size needed.
        // 2. Align to 16 bytes for MFC DMA performance.
        // 3. Ensure we don't read past the end of the items array or the PPU source memory.
        uint32_t bytes_to_read = n * sizeof(SPU_SortItem);
        uint32_t xfer_size = (bytes_to_read + 15) & ~15;

        if (xfer_size > (MAX_ITEMS * sizeof(SPU_SortItem))) {
            xfer_size = MAX_ITEMS * sizeof(SPU_SortItem);
        }

        if (xfer_size > 0 && ctx.ea_items != 0) {
            for (uint32_t offset = 0; offset < xfer_size; offset += 16384) {
                uint32_t chunk = xfer_size - offset;
                if (chunk > 16384) chunk = 16384;
                mfc_get((uint8_t*)items + offset, ctx.ea_items + offset, chunk, 31, 0, 0);
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
                            for (int i = left; i < n; i++)
                                dst[i] = src[i];
                            break;
                        }
                        if (right > n)
                            right = n;

                        int i = left, j = mid, k = left;
                        while (i < mid && j < right) {
                            if (src[i].z <= src[j].z) {
                                dst[k++] = src[i++];
                            } else {
                                dst[k++] = src[j++];
                            }
                        }
                        while (i < mid)
                            dst[k++] = src[i++];
                        while (j < right)
                            dst[k++] = src[j++];
                    }
                    SPU_SortItem* tmp = src;
                    src = dst;
                    dst = tmp;
                }

                if (src != items) {
                    for (int i = 0; i < n; ++i)
                        items[i] = src[i];
                }
            }

            for (uint32_t offset = 0; offset < xfer_size; offset += 16384) {
                uint32_t chunk = xfer_size - offset;
                if (chunk > 16384) chunk = 16384;
                mfc_put((uint8_t*)items + offset, ctx.ea_items + offset, chunk, 31, 0, 0);
            }
            mfc_read_tag_status_all();
        }

        ctx.run_command = 0;
        ctx.status = 0;
        mfc_put(&ctx, ctx_ea, sizeof(SortContext), 31, 0, 0);
        mfc_read_tag_status_all();
    }

    return 0;
}

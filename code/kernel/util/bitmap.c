#include "bitmap.h"

#include "assert.h"
#include "slab.h"

/* 将位图btmp初始化 */
struct bitmap* bitmap_init(size_t nr) {
    struct bitmap* btmp = kmalloc(sizeof(*btmp));
    assert(btmp != nullptr);
    btmp->nr = nr;
    btmp->btmp_bytes_len = (nr + 7) / 8;
    btmp->bits = (volatile uint8_t*)(kmalloc(btmp->btmp_bytes_len));
    assert(btmp->bits != nullptr);
    while(nr < btmp->btmp_bytes_len * 8){
        bitmap_set(btmp, nr++, 1);
    }
    initlock(&btmp->bitmap_lock, "bitmap_lock");
    return btmp;
}

bool bitmap_scan_test(struct bitmap* btmp, size_t bit_idx) {
    assert(bit_idx < btmp->nr);
    return test_bit(bit_idx, btmp->bits);
}

long bitmap_scan_set(struct bitmap* btmp, size_t cnt) {
    acquire(&btmp->bitmap_lock);
    long idx_byte = 0;
    while ((0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)) { idx_byte++; }
    assert(idx_byte < btmp->btmp_bytes_len);
    if (idx_byte == btmp->btmp_bytes_len) { return -1; }
    long idx_bit = 0;
    while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) { idx_bit++; }
    long bit_idx_start = idx_byte * 8 + idx_bit;
    if (cnt == 1) {
        bitmap_set(btmp, bit_idx_start, 1);
        release(&btmp->bitmap_lock);
        return bit_idx_start; 
    }
    size_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start);
    size_t next_bit = bit_idx_start + 1;
    size_t count = 1;
    bit_idx_start = -1;
    while (bit_left-- > 0) {
        if (!(bitmap_scan_test(btmp, next_bit))) {
            count++;
        } else {
            count = 0;
        }
        if (count == cnt) {
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    if (bit_idx_start != -1) {
        size_t num = 0;
        while (num < cnt) { bitmap_set(btmp, bit_idx_start + num++, 1); }
    }
    release(&btmp->bitmap_lock);
    return bit_idx_start;
}


void bitmap_set(struct bitmap* btmp, size_t bit_idx, long value) {
    assert((value == 0) || (value == 1));
    assert(bit_idx < btmp->nr);
    if (value) {
        test_and_set_bit(bit_idx, btmp->bits);
    } else {
        test_and_clear_bit(bit_idx, btmp->bits);
    }
}

void bitmap_scan_clear(struct bitmap* btmp, size_t bit_idx, size_t cnt) {
    acquire(&btmp->bitmap_lock);
    if (cnt == 1) {
        assert(bitmap_scan_test(btmp, bit_idx) == 1);
        bitmap_set(btmp, bit_idx, 0);
    }
    else{
        size_t num = 0;
        while (num < cnt) { 
            assert(bitmap_scan_test(btmp, bit_idx + num) == 1);
            bitmap_set(btmp, bit_idx + num++, 0); 
        }
    }
    release(&btmp->bitmap_lock);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define PAGE_SIZE 4096
#define NUM_FRAMES 3
#define MAX_REF_LENGTH 30

/*PAGING SYSTEM - VIRTUAL ADDRESS TRANSLATION */

typedef struct {
    unsigned int page_number;
    unsigned int offset;
} AddressTranslation;

AddressTranslation translate_address(unsigned int virtual_address) {
    AddressTranslation result;
    result.page_number = virtual_address / PAGE_SIZE;
    result.offset = virtual_address % PAGE_SIZE;
    return result;
}

void demo_paging_system() {
    printf("\n PAGING SYSTEM (PAGE_SIZE = %d bytes) \n", PAGE_SIZE);
    unsigned int sample_addresses[] = {0, 4096, 8192, 5000, 10500, 4097};
    int count = sizeof(sample_addresses) / sizeof(sample_addresses[0]);

    printf("%-18s %-14s %-10s\n", "Virtual Address", "Page Number", "Offset");
    for (int i = 0; i < count; i++) {
        AddressTranslation t = translate_address(sample_addresses[i]);
        printf("%-18u %-14u %-10u\n", sample_addresses[i], t.page_number, t.offset);
    }
}

/*FIFO PAGE REPLACEMENT*/

typedef struct {
    int page_faults;
    int page_hits;
    double hit_ratio;
    double miss_ratio;
} SimResult;

int is_in_frames(int frames[], int num_frames, int page) {
    for (int i = 0; i < num_frames; i++)
        if (frames[i] == page) return 1;
    return 0;
}

void print_frames(int frames[], int num_frames) {
    printf("[ ");
    for (int i = 0; i < num_frames; i++) {
        if (frames[i] == -1) printf("_ ");
        else printf("%d ", frames[i]);
    }
    printf("]");
}

SimResult run_fifo(int ref_string[], int ref_length, int num_frames, int verbose) {
    int frames[num_frames];
    for (int i = 0; i < num_frames; i++) frames[i] = -1;

    int fifo_index = 0;   /* points to the oldest frame, replaced next */
    SimResult result = {0, 0, 0.0, 0.0};

    if (verbose) printf("\n--- FIFO Page Replacement Log (frames = %d) ---\n", num_frames);

    for (int i = 0; i < ref_length; i++) {
        int page = ref_string[i];

        if (is_in_frames(frames, num_frames, page)) {
            result.page_hits++;
            if (verbose) {
                printf("Access %-2d Page %-2d -> HIT   Frames: ", i + 1, page);
                print_frames(frames, num_frames);
                printf("\n");
            }
        } else {
            result.page_faults++;
            int evicted = frames[fifo_index];
            frames[fifo_index] = page;
            fifo_index = (fifo_index + 1) % num_frames;

            if (verbose) {
                printf("Access %-2d Page %-2d -> FAULT (evicted %-2d) Frames: ",
                       i + 1, page, evicted);
                print_frames(frames, num_frames);
                printf("\n");
            }
        }
    }
    result.hit_ratio = (double)result.page_hits / ref_length;
    result.miss_ratio = (double)result.page_faults / ref_length;
    return result;
}

  /* PART 3: LRU PAGE REPLACEMENT */

SimResult run_lru(int ref_string[], int ref_length, int num_frames, int verbose) {
    int frames[num_frames];
    int last_used[num_frames];   /* "time" each frame's page was last accessed */
    for (int i = 0; i < num_frames; i++) {
        frames[i] = -1;
        last_used[i] = -1;
    }

    SimResult result = {0, 0, 0.0, 0.0};

    if (verbose) printf("\n--- LRU Page Replacement Log (frames = %d) ---\n", num_frames);

    for (int i = 0; i < ref_length; i++) {
        int page = ref_string[i];
        int found_index = -1;

        for (int f = 0; f < num_frames; f++) {
            if (frames[f] == page) { found_index = f; break; }
        }

        if (found_index != -1) {
            result.page_hits++;
            last_used[found_index] = i;   /* mark as most recently used */
            if (verbose) {
                printf("Access %-2d Page %-2d -> HIT   Frames: ", i + 1, page);
                print_frames(frames, num_frames);
                printf("\n");
            }
        } else {
            result.page_faults++;

            /* find an empty frame first */
            int target = -1;
            for (int f = 0; f < num_frames; f++) {
                if (frames[f] == -1) { target = f; break; }
            }

            /* no empty frame -> find the least recently used one */
            if (target == -1) {
                int oldest_time = last_used[0];
                target = 0;
                for (int f = 1; f < num_frames; f++) {
                    if (last_used[f] < oldest_time) {
                        oldest_time = last_used[f];
                        target = f;
                    }
                }
            }

            int evicted = frames[target];
            frames[target] = page;
            last_used[target] = i;

            if (verbose) {
                printf("Access %-2d Page %-2d -> FAULT (evicted %-2d) Frames: ",
                       i + 1, page, evicted);
                print_frames(frames, num_frames);
                printf("\n");
            }
        }
    }

    result.hit_ratio = (double)result.page_hits / ref_length;
    result.miss_ratio = (double)result.page_faults / ref_length;
    return result;
}

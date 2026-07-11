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


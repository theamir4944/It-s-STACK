#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>

#include "colors.h"
#include "stack_logging.cpp"


// #define STACK_DEBUG


typedef double type;
const type POIZON = NAN;
const type CANARY_VAL = 0xBEE;
const size_t MIN_POP_CAP = 2;

#define CANARIES
#define HASHES
#define VERIFIES

#define SPECIFICATOR "%5lg"
#define MAIN_STACK_INFO " \t  File: %s \n \t  Function: %s \n \t  Line: %d \n \t "
#define MAIN_STACK_ARGS __FILE__, __PRETTY_FUNCTION__, __LINE__

#define identity(a, b) (isnan(a) && isnan(b)) || (a == b)

#ifdef STACK_DEBUG
#define ON_DBG(...) __VA_ARGS__
#else
#define ON_DBG(...)
#endif // STACK_DEBUG


enum stack_error {

    STACK_OK = 0,
    STACK_NO_MEMORY,
    STACK_NULLPTR,
    STACK_WRONG_VAL

};


struct stack_debug_info {
    char* stack_name;
    char* file_name;
    size_t init_line;
};


struct stack_t {
    ON_DBG(
        stack_debug_info debug_data;
    )
    type* real_buffer;
    type* data;
    ssize_t size;
    ssize_t capacity;
#ifdef CANARIES
    type* canary_first;
    type* canary_last;
#endif // CANARIES
#ifdef HASHES
    uint64_t hash;
#endif // HASHES
};


void struct_null(stack_t* stack);
stack_error stack_constructor(stack_t* stack, ssize_t capacity ON_DBG(, char* stackname, char* filename, size_t initline));
stack_error stack_raise(stack_t* stack);
stack_error stack_push(stack_t* stack, type new_value);
stack_error stack_cut(stack_t* stack);
stack_error stack_pop(stack_t* stack, type* output_value);
stack_error stack_destructor(stack_t* stack);
uint64_t hash_djb2(stack_t* stack);
void stack_dump(stack_t* stack);
bool stack_verify(stack_t* stack);
void stack_print(stack_t* stack);
bool stack_user_verify(stack_t* stack);


void struct_null(stack_t* stack) {

    stack->real_buffer = NULL;
    stack->data = NULL;
    stack->size = 0;
    stack->capacity = 0;
#ifdef CANARIES
    stack->canary_first = NULL;
    stack->canary_last = NULL;
#endif // CANARIES
#ifdef HASHES
    stack->hash = hash_djb2(stack);
#endif // HASHES
#ifdef STACK_DEBUG
    stack->debug_data.file_name = NULL;
    stack->debug_data.stack_name = NULL;
    stack->debug_data.init_line = 0;
#endif // STACK_DEBUG

}


stack_error stack_constructor(stack_t* stack, ssize_t capacity ON_DBG(, char* stackname, char* filename, size_t initline)) {

    assert(stack);
    assert(stack->data == NULL);
    assert(stack->real_buffer == NULL);
    assert(capacity > 0);

    log_info("Started to constructing a stack %p \n" MAIN_STACK_INFO, stack, MAIN_STACK_ARGS);

    struct_null(stack);

#ifdef STACK_DEBUG

    assert(stackname && *stackname);
    assert(filename && *filename);
    assert(initline > 0);

    (stack->debug_data).stack_name = stackname;
    (stack->debug_data).file_name = filename;
    (stack->debug_data).init_line = initline;

#endif // STACK_DEBUG

    stack->real_buffer = (type*) calloc (capacity + 2, sizeof(type));

    if (stack->real_buffer) {
        stack->data = stack->real_buffer + 1;
        stack->capacity = capacity;
        stack->size = 0;
#ifdef CANARIES
        stack->canary_first = stack->real_buffer;
        *(stack->canary_first) = CANARY_VAL;
        stack->canary_last = stack->real_buffer + stack->capacity + 1;
        *(stack->canary_last) = CANARY_VAL;
#endif // CANARIES
        for (ssize_t index = stack->size; index < capacity; index++) {
            (stack->data)[index] = POIZON;
        }
#ifdef HASHES
        stack->hash = hash_djb2(stack);
#endif // HASHES

        log_ok("Stack %p created ; full stack-buffer: %p \n" MAIN_STACK_INFO, stack, stack->real_buffer, MAIN_STACK_ARGS);
        return STACK_OK;
    }
    else {
        log_error("Can't to calloc memory for stack %p \n" MAIN_STACK_INFO, stack, MAIN_STACK_ARGS);
        return STACK_NO_MEMORY;
    }

}


stack_error stack_raise(stack_t* stack) {

    type* new_real_buffer = (type*) realloc (stack->real_buffer, (stack->capacity * 2) * sizeof(type) + 2 * sizeof(type));
    
    if (new_real_buffer != NULL) {
        stack->capacity = stack->capacity * 2;
        stack->real_buffer = new_real_buffer;
        stack->data = stack->real_buffer + 1;
        new_real_buffer = NULL;

        for (ssize_t index = stack->size; index < stack->capacity; index++) {
            (stack->data)[index] = POIZON;
        }
#ifdef CANARIES
        stack->canary_first = stack->real_buffer;
        *(stack->canary_first) = CANARY_VAL;
        stack->canary_last = stack->real_buffer + stack->capacity + 1;
        *(stack->canary_last) = CANARY_VAL;
#endif // CANARIES
        log_ok("Stack %p raised correctly ; stack-data: %p ; new capacity: %zd \n" MAIN_STACK_INFO, stack, stack->data, stack->capacity, MAIN_STACK_ARGS);

        return STACK_OK;
    }

    else {
        log_error("Can't to raise stack %p ; stack-data: %p \n" MAIN_STACK_INFO, stack, stack->data, MAIN_STACK_ARGS);
        return STACK_NULLPTR;
    }

}


stack_error stack_push(stack_t* stack, type new_value) {

    assert(stack_user_verify(stack));

    log_info("Start pushing to stack %p ; stack-data: %p ; size: %zd ; capacity: %zd \n" MAIN_STACK_INFO, stack, stack->data, stack->size, stack->capacity, MAIN_STACK_ARGS);

    if (stack->capacity <= 0) {
        log_error("Capacity = %zd \n" MAIN_STACK_INFO, stack->capacity, MAIN_STACK_ARGS);
        return STACK_WRONG_VAL;
    }

    if (stack->size >= stack->capacity) {

        log_info("Starting to raising the stack %p ; stack-data: %p \n" MAIN_STACK_INFO, stack, stack->data, MAIN_STACK_ARGS);
        if (stack_raise(stack) != STACK_OK) {
            return STACK_NULLPTR;
        }
        
    }

    *(stack->data + stack->size) = new_value;
    log_info("Got new value:" SPECIFICATOR, new_value);
    log_info("New value in data:" SPECIFICATOR "\n", *(stack->data + stack->size));
    (stack->size)++;

    log_ok("Stack %p pushing done ; stack-data: %p ; size: %zd ; capacity: %zd \n" MAIN_STACK_INFO, stack, stack->data, stack->size, stack->capacity, MAIN_STACK_ARGS);
    stack->hash = hash_djb2(stack);
    return STACK_OK;

}


stack_error stack_cut(stack_t* stack) {

    type* new_real_buffer = (type*) realloc (stack->real_buffer, (stack->capacity / 2) * sizeof(type) + 2 * sizeof(type));

    if (new_real_buffer == NULL) {
        log_error("Can't to cut stack %p ; stack-data: %p \n" MAIN_STACK_INFO, stack, stack->data, MAIN_STACK_ARGS);
        return STACK_NULLPTR;
    }
    else {
        stack->capacity = stack->capacity / 2;
        stack->real_buffer = new_real_buffer;
        stack->data = new_real_buffer + 1;
        new_real_buffer = NULL;
#ifdef CANARIES
        stack->canary_first = stack->real_buffer;
        *(stack->canary_first) = CANARY_VAL;
        stack->canary_last = stack->real_buffer + stack->capacity + 1;
        *(stack->canary_last) = CANARY_VAL;
#endif // CANARIES
        log_ok("Stack %p cut correctly ; stack-data: %p ; new capacity: %zd \n" MAIN_STACK_INFO, stack, stack->data, stack->capacity, MAIN_STACK_ARGS);
        return STACK_OK;
    }

}


stack_error stack_pop(stack_t* stack, type* output_value) {

    assert(stack_user_verify(stack));
    assert(output_value);

    log_info("Start popping from stack %p ; stack-data: %p ; size: %zd ; capacity: %zd \n" MAIN_STACK_INFO, stack, stack->data, stack->size, stack->capacity, MAIN_STACK_ARGS);

    stack_error is_error = STACK_OK;

    if (stack->size <= 0) {
        log_error("Size = %zd ; Stack = %p ; stack-data = %p \n" MAIN_STACK_INFO, stack->size, stack, stack->data, MAIN_STACK_ARGS);
        return STACK_WRONG_VAL;
    }

    *output_value = *(stack->data + stack->size - 1);
    *(stack->data + stack->size - 1) = POIZON;
    (stack->size)--;

    if ((stack->size <= stack->capacity / 4) && (stack->capacity >= MIN_POP_CAP)) {
        log_info("Starting to cutting the stack %p ; stack-data: %p \n" MAIN_STACK_INFO, stack, stack->data, MAIN_STACK_ARGS);
        is_error = stack_cut(stack);
    }

    log_ok("Stack %p popping done ; stack-data: %p ; size: %zd ; capacity: %zd \n" MAIN_STACK_INFO, stack, stack->data, stack->size, stack->capacity, MAIN_STACK_ARGS);
    stack->hash = hash_djb2(stack);
    return is_error;

}


stack_error stack_destructor(stack_t* stack) {

    log_info("Starting Stack %p destroy \n" MAIN_STACK_INFO, stack, MAIN_STACK_ARGS);

    if (stack == NULL) {
        log_ok("Stack has been already destroyed \n" MAIN_STACK_INFO, MAIN_STACK_ARGS);
        return STACK_NULLPTR;
    }

    if (stack->real_buffer) {
        memset(stack->real_buffer, '\0', (stack->capacity + 2) * sizeof(type));
        free(stack->real_buffer);
        stack->real_buffer = NULL;
    }

    memset(stack, '\0', sizeof(stack_t));
    free(stack);
    log_ok("Stack destroyed \n" MAIN_STACK_INFO, MAIN_STACK_ARGS);
    return STACK_OK;

}


uint64_t hash_djb2(stack_t* stack) {

    char* stack_copy = (char*) stack;
    uint64_t hash = 0;

    for (size_t index = 0; index < sizeof(stack_t) - sizeof(uint64_t); index++) {
        hash = ((hash << 5) + hash) + (int)(stack_copy[index]);
    }

    return hash;

}


void stack_dump(stack_t* stack) {

    log("\n\n[DUMPING]");
    log(" Main information:\n");
    log(" \t  Stack: %p ; Capacity: %zd ; Size: %zd ; Data = %p \n", stack, stack->capacity, stack->size, stack->data);
    
    for (size_t index = 0; index < stack->size; index++) {
        log("\n\t  data[%5zu] = " SPECIFICATOR, index, (stack->data)[index]);
    }
    
    for (size_t index = stack->size; index < stack->capacity; index++) {
        log("\n\t  data[%5zu] = " SPECIFICATOR, index, (stack->data)[index]);

        if (! identity((stack->data)[index], POIZON)) {
            log("\t!!!");
        }
        else {
            log("\t*POISON");
        }

    }
#ifdef CANARIES
    log("\n\n \t  Canary-first = " SPECIFICATOR " ; Canary-end = " SPECIFICATOR, *(stack->canary_first), *(stack->canary_last));
#endif // CANARIES
    log("\n\n" MAIN_STACK_INFO, MAIN_STACK_ARGS);
    return;

}


bool stack_verify(stack_t* stack) {

    log_info("Veryfying stack %p started \n" MAIN_STACK_INFO, stack, MAIN_STACK_ARGS);

    if (stack == NULL) {
        log_error("Stack = %p \n" MAIN_STACK_INFO, stack, MAIN_STACK_ARGS);
        return false;
    }

    if (stack->real_buffer == NULL) {
        log_error("No buffer in stack %p ; buffer = %p \n" MAIN_STACK_INFO, stack, stack->real_buffer, MAIN_STACK_ARGS);
        return false;
    }

    if (stack->data == NULL) {
        log_error("No stack-data in stack %p ; stack-data = %p \n" MAIN_STACK_INFO, stack, stack->data, MAIN_STACK_ARGS);
        return false;
    }

    if (stack->data != stack->real_buffer + 1) {
        log_error("Stack-data was removed ; stack = %p ; stack-data = %p \n" MAIN_STACK_INFO, stack, stack->data, MAIN_STACK_ARGS);
        return false;
    }

    if (stack->capacity <= 0) {
        log_error("Capacity = %zd \n" MAIN_STACK_INFO, stack->capacity, MAIN_STACK_ARGS);
        return false;
    }

    if (stack->size < 0) {
        log_error("Size = %zd \n" MAIN_STACK_INFO, stack->size, MAIN_STACK_ARGS);
        return false;
    }

    if (stack->size > stack->capacity) {
        log_error("Capacity = %zd < Size = %zd \n" MAIN_STACK_INFO, stack->capacity, stack->size, MAIN_STACK_ARGS);
        return false;
    }
#ifdef CANARIES
    if (stack->canary_first == NULL || stack->canary_last == NULL) {
        log_error("Canary-first = %p ; canary-last = %p \n" MAIN_STACK_INFO, stack->canary_first, stack->canary_last, MAIN_STACK_ARGS);
        return false;
    }

    if (*(stack->canary_first) != CANARY_VAL || *(stack->canary_last) != CANARY_VAL) {
        log_error("Canary-value = " SPECIFICATOR " ; Canary-first = " SPECIFICATOR " ; Canary-last = " SPECIFICATOR "\n" MAIN_STACK_INFO, CANARY_VAL, *(stack->canary_first), *(stack->canary_last), MAIN_STACK_ARGS);
        return false;
    }

    if (stack->canary_first != stack->real_buffer || stack->canary_last != stack->real_buffer + stack->capacity + 1) {
        log_error("Canary was removed ; Canary-first = %p ; Canary-last = %p ; stack-buffer = %p ; stack-end-buffer = %p \n" MAIN_STACK_INFO, stack->canary_first, stack->canary_last, stack->real_buffer, stack->real_buffer + stack->capacity + 1, MAIN_STACK_ARGS);
        return false;
    }
#endif // CANARIES
#ifdef HASHES
    if (stack->hash != hash_djb2(stack)) {
        log_error("Hash in struct = " PRIu64 " ; Real hash = " PRIu64 " \n", stack->hash, hash_djb2(stack));
        return false;
    }
#endif // HASHES
#ifdef STACK_DEBUG

    if (stack->debug_data.file_name == NULL) {
        log_error("FILE NAME = %p \n" MAIN_STACK_INFO, stack->debug_data.file_name, MAIN_STACK_ARGS);
        return false;
    }

    if (*(stack->debug_data.file_name) == '\0') {
        log_error("Wrong file name \n" MAIN_STACK_INFO, MAIN_STACK_ARGS);
        return false;
    }

    if (stack->debug_data.stack_name == NULL) {
        log_error("STACK NAME = %p \n" MAIN_STACK_INFO, stack->debug_data.stack_name, MAIN_STACK_ARGS);
        return false;
    }

    if (*(stack->debug_data.stack_name) == '\0') {
        log_error("Wrong stack name \n" MAIN_STACK_INFO, MAIN_STACK_ARGS);
        return false;
    }

    if (stack->debug_data.init_line <= 0) {
        log_error("INIT LINE = %zd \n" MAIN_STACK_INFO, stack->debug_data.init_line, MAIN_STACK_ARGS);
        return false;
    }

#endif

    log_ok("Stack %p is okay \n" MAIN_STACK_INFO, stack, MAIN_STACK_ARGS);
    return true;

}


void stack_print(stack_t* stack) {

    printf("Stack:\n");
    for (size_t index = 0; index < stack->size; index++) {
        printf("[%*zu]\t", 5, index);
    }
    printf("\n");
    for (size_t index = 0; index < stack->size; index++) {
        printf(" " SPECIFICATOR " \t", (stack->data)[index]);
    }
    printf("\n\n");

}


bool stack_user_verify(stack_t* stack) {

#ifdef VERIFIES
    return stack_verify(stack);
#endif // VERIFIES
    return true;

}


void byte_swap(char* list, ssize_t ind_1, ssize_t ind_2, size_t size_of) {

    assert(list != NULL);
    assert(size_of >= 1);

    if (ind_1 == ind_2) { return; }

    for (size_t delta = 0; delta < size_of; delta++) {

        char temp = *(list + ind_1 * size_of + delta);
        *(list + ind_1 * size_of + delta) = *(list + ind_2 * size_of + delta);
        *(list + ind_2 * size_of + delta) = temp;

    }

    return;

}


void quick_sort(void* list, ssize_t left, ssize_t right, size_t size_of) {

    assert(list != NULL);
    assert(size_of >= 1);

    if (left >= right) { return; }

    ssize_t ind_compare = left + (rand() % (1 + right - left));
    
    char* p_list = (char*)list;
    byte_swap(p_list, left, ind_compare, size_of);
    ssize_t last = left;
    
    for (ssize_t i = left + 1; i <= right; i++) {
        
        if (*(p_list + i * size_of) < *(p_list + left * size_of)) { 
            last++;
            byte_swap(p_list, i, last, size_of); 
        }
        
    }
    
    byte_swap(p_list, left, last, size_of);
    
    quick_sort(list, left, last - 1, size_of);
    quick_sort(list, last + 1, right, size_of);

}


void full_qsort(void* list, ssize_t left, ssize_t right, size_t size_of) {

    assert(list != NULL);
    assert(size_of >= 1);

    time_t seed = time(NULL);
    srand(seed);
    quick_sort(list, left, right, size_of);

}

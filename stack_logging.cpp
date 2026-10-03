#include <stdio.h>
#include <assert.h>
#include <stdarg.h>


FILE* GLOBAL_LOG_FILE = NULL;


#define log_error(pattern, ...) log("\n\n[ERROR] - " pattern, ##__VA_ARGS__)
#define log_info(pattern, ...)  log("\n\n[INFO]  - " pattern, ##__VA_ARGS__)
#define log_ok(pattern, ...)    log("\n\n[OK]    - " pattern, ##__VA_ARGS__)


void log_init(const char* filename);
void log(const char* pattern, ...);
void log_destroy(void);


void log_init(const char* filename) {

    assert(filename != NULL);

    if (GLOBAL_LOG_FILE != NULL) {
        log_error("Trying to open another file!\nFunction: log_init ; GLOBAL_LOG_FILE = %p != NULL", GLOBAL_LOG_FILE);
        return;
    }

    GLOBAL_LOG_FILE = fopen(filename, "w");

    assert(GLOBAL_LOG_FILE != NULL);

    setbuf(GLOBAL_LOG_FILE, NULL); // for Abort-situation

}


void log(const char* pattern, ...) {

    if (GLOBAL_LOG_FILE == NULL) {
        return;
    }

    assert(pattern != NULL);
    assert(GLOBAL_LOG_FILE != NULL);

    va_list ap = { };
    va_start(ap, pattern);

    vfprintf(GLOBAL_LOG_FILE, pattern, ap);

    va_end(ap);
    
}


void log_destroy(void) {

    if (GLOBAL_LOG_FILE == NULL) {
        return;
    }

    assert(GLOBAL_LOG_FILE != NULL);

    fclose(GLOBAL_LOG_FILE);
    GLOBAL_LOG_FILE = NULL;

}

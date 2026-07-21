#ifndef cJSON_H
#define cJSON_H

/*
 * Minimal cJSON for PS4 Yandex Music Player
 * Based on cJSON library (MIT License)
 * https://github.com/DaveGamble/cJSON
 */

#ifdef __cplusplus
extern "C" {
#endif

/* cJSON types */
#define cJSON_Invalid 0
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)
#define cJSON_Raw    (1 << 7)

/* The cJSON structure: */
typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    
    int type;
    
    char *valuestring;
    int valueint;
    double valuedouble;
    
    char *string;
} cJSON;

/* cJSON Hooks */
typedef struct cJSON_Hooks {
    void *(*malloc_fn)(size_t sz);
    void (*free_fn)(void *ptr);
} cJSON_Hooks;

/* Get version */
const char* cJSON_Version(void);

/* Memory management */
void cJSON_InitHooks(cJSON_Hooks* hooks);
cJSON *cJSON_New_Item(void);

/* Parse a JSON string */
cJSON *cJSON_Parse(const char *value);

/* Render a cJSON entity to text */
char  *cJSON_Print(const cJSON *item);
char  *cJSON_PrintUnformatted(const cJSON *item);

/* Delete a cJSON entity */
void    cJSON_Delete(cJSON *c);

/* Get item from object/array by string index */
cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string);

/* Get item from array by index */
cJSON *cJSON_GetArrayItem(const cJSON * const array, int index);

/* Get array size */
int cJSON_GetArraySize(const cJSON * const array);

/* Check type */
static inline int cJSON_IsNumber(const cJSON * const item) {
    if (item == NULL) return 0;
    return (item->type & 0xFF) == cJSON_Number;
}

static inline int cJSON_IsString(const cJSON * const item) {
    if (item == NULL) return 0;
    return (item->type & 0xFF) == cJSON_String;
}

static inline int cJSON_IsArray(const cJSON * const item) {
    if (item == NULL) return 0;
    return (item->type & 0xFF) == cJSON_Array;
}

static inline int cJSON_IsObject(const cJSON * const item) {
    if (item == NULL) return 0;
    return (item->type & 0xFF) == cJSON_Object;
}

static inline int cJSON_IsBool(const cJSON * const item) {
    if (item == NULL) return 0;
    return (item->type & (cJSON_True | cJSON_False)) != 0;
}

#ifdef __cplusplus
}
#endif

#endif /* cJSON_H */

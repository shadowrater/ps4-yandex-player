/*
 * Minimal cJSON implementation for PS4
 * Based on cJSON library (MIT License)
 * Only the functions needed for Yandex Music API parsing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "cJSON.h"

#define cJSON_MAJOR_VERSION 1
#define cJSON_MINOR_VERSION 7
#define cJSON_PATCH_VERSION 18

const char* cJSON_Version(void) {
    static char version[16];
    sprintf(version, "%d.%d.%d", cJSON_MAJOR_VERSION, cJSON_MINOR_VERSION, cJSON_PATCH_VERSION);
    return version;
}

/* ─── Internal helpers ────────────────────────────────────────── */

static void *internal_malloc(size_t sz) { return malloc(sz); }
static void internal_free(void *ptr)    { free(ptr); }

static void *(*cJSON_malloc)(size_t sz) = internal_malloc;
static void  (*cJSON_free)(void *ptr)   = internal_free;

void cJSON_InitHooks(cJSON_Hooks* hooks) {
    if (!hooks) { cJSON_malloc = internal_malloc; cJSON_free = internal_free; return; }
    cJSON_malloc = hooks->malloc_fn ? hooks->malloc_fn : internal_malloc;
    cJSON_free   = hooks->free_fn   ? hooks->free_fn   : internal_free;
}

cJSON *cJSON_New_Item(void) {
    cJSON *node = (cJSON *)cJSON_malloc(sizeof(cJSON));
    if (node) memset(node, 0, sizeof(cJSON));
    return node;
}

/* ─── String helpers ──────────────────────────────────────────── */

static int cJSON_strcasecmp(const char *s1, const char *s2) {
    if (!s1) return (s1 == s2) ? 0 : 1;
    if (!s2) return 1;
    for (; *s1 && *s2 && (*s1 == *s2 || (*s1 >= 'A' && *s1 <= 'Z' && *s1 + 32 == *s2) ||
        (*s2 >= 'A' && *s2 <= 'Z' && *s2 + 32 == *s1)); s1++, s2++);
    return *s1 - *s2;
}

static char *cJSON_strdup(const char *str) {
    size_t len = strlen(str) + 1;
    char *copy = (char *)cJSON_malloc(len);
    if (copy) memcpy(copy, str, len);
    return copy;
}

/* ─── Parser ──────────────────────────────────────────────────── */

typedef struct {
    const char *s;
    int pos;
} parse_buffer;

static void skip_whitespace(parse_buffer *p) {
    while (p->s[p->pos] == ' ' || p->s[p->pos] == '\t' || 
           p->s[p->pos] == '\n' || p->s[p->pos] == '\r') {
        p->pos++;
    }
}

static char parse_escape_char(parse_buffer *p) {
    p->pos++; // skip backslash
    char c = p->s[p->pos++];
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '"': return '"';
        case '/': return '/';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'u': {
            // Parse 4 hex digits (simplified - ASCII only)
            char hex[5] = {0};
            for (int i = 0; i < 4 && p->s[p->pos]; i++)
                hex[i] = p->s[p->pos++];
            return (char)strtol(hex, NULL, 16);
        }
        default: return c;
    }
}

static char *parse_string(parse_buffer *p) {
    if (p->s[p->pos] != '"') return NULL;
    p->pos++; // skip opening quote
    
    int start = p->pos;
    while (p->s[p->pos] && p->s[p->pos] != '"') {
        if (p->s[p->pos] == '\\') p->pos++; // skip escaped char
        p->pos++;
    }
    
    if (p->s[p->pos] != '"') return NULL;
    
    int len = p->pos - start;
    char *str = (char *)cJSON_malloc(len + 1);
    if (!str) return NULL;
    
    // Copy with escape handling
    int j = 0;
    for (int i = start; i < start + len; i++) {
        if (p->s[i] == '\\') {
            str[j++] = parse_escape_char(&(parse_buffer){p->s, i + 1});
            i++; // skip the escaped char
        } else {
            str[j++] = p->s[i];
        }
    }
    str[j] = '\0';
    p->pos++; // skip closing quote
    return str;
}

static double parse_number(parse_buffer *p) {
    char *end;
    double val = strtod(p->s + p->pos, &end);
    p->pos = (int)(end - p->s);
    return val;
}

static cJSON *parse_value(parse_buffer *p);

static cJSON *parse_array(parse_buffer *p) {
    p->pos++; // skip '['
    skip_whitespace(p);
    
    cJSON *array = cJSON_New_Item();
    if (!array) return NULL;
    array->type = cJSON_Array;
    
    cJSON *child = NULL;
    
    if (p->s[p->pos] == ']') {
        p->pos++;
        return array;
    }
    
    while (1) {
        cJSON *item = parse_value(p);
        if (!item) { cJSON_Delete(array); return NULL; }
        
        if (!child) {
            array->child = item;
            child = item;
        } else {
            child->next = item;
            item->prev = child;
            child = item;
        }
        
        skip_whitespace(p);
        if (p->s[p->pos] == ',') {
            p->pos++;
            skip_whitespace(p);
        } else if (p->s[p->pos] == ']') {
            p->pos++;
            return array;
        } else {
            cJSON_Delete(array);
            return NULL;
        }
    }
}

static cJSON *parse_object(parse_buffer *p) {
    p->pos++; // skip '{'
    skip_whitespace(p);
    
    cJSON *object = cJSON_New_Item();
    if (!object) return NULL;
    object->type = cJSON_Object;
    
    cJSON *child = NULL;
    
    if (p->s[p->pos] == '}') {
        p->pos++;
        return object;
    }
    
    while (1) {
        skip_whitespace(p);
        char *key = parse_string(p);
        if (!key) { cJSON_Delete(object); return NULL; }
        
        skip_whitespace(p);
        if (p->s[p->pos] != ':') { cJSON_free(key); cJSON_Delete(object); return NULL; }
        p->pos++;
        skip_whitespace(p);
        
        cJSON *item = parse_value(p);
        if (!item) { cJSON_free(key); cJSON_Delete(object); return NULL; }
        
        item->string = key;
        
        if (!child) {
            object->child = item;
            child = item;
        } else {
            child->next = item;
            item->prev = child;
            child = item;
        }
        
        skip_whitespace(p);
        if (p->s[p->pos] == ',') {
            p->pos++;
            skip_whitespace(p);
        } else if (p->s[p->pos] == '}') {
            p->pos++;
            return object;
        } else {
            cJSON_Delete(object);
            return NULL;
        }
    }
}

static cJSON *parse_value(parse_buffer *p) {
    skip_whitespace(p);
    
    if (!p->s[p->pos]) return NULL;
    
    char c = p->s[p->pos];
    
    if (c == '"') {
        cJSON *item = cJSON_New_Item();
        if (!item) return NULL;
        item->type = cJSON_String;
        item->valuestring = parse_string(p);
        if (!item->valuestring) { cJSON_free(item); return NULL; }
        return item;
    }
    
    if (c == '{') {
        return parse_object(p);
    }
    
    if (c == '[') {
        return parse_array(p);
    }
    
    if (c == 't' || c == 'f' || c == 'n') {
        cJSON *item = cJSON_New_Item();
        if (!item) return NULL;
        
        if (strncmp(p->s + p->pos, "true", 4) == 0) {
            item->type = cJSON_True;
            item->valueint = 1;
            p->pos += 4;
        } else if (strncmp(p->s + p->pos, "false", 5) == 0) {
            item->type = cJSON_False;
            item->valueint = 0;
            p->pos += 5;
        } else if (strncmp(p->s + p->pos, "null", 4) == 0) {
            item->type = cJSON_NULL;
            p->pos += 4;
        }
        
        return item;
    }
    
    if (c == '-' || (c >= '0' && c <= '9')) {
        cJSON *item = cJSON_New_Item();
        if (!item) return NULL;
        item->type = cJSON_Number;
        item->valuedouble = parse_number(p);
        item->valueint = (int)item->valuedouble;
        return item;
    }
    
    return NULL;
}

cJSON *cJSON_Parse(const char *value) {
    if (!value) return NULL;
    
    parse_buffer p = {value, 0};
    cJSON *result = parse_value(&p);
    return result;
}

/* ─── Render ──────────────────────────────────────────────────── */

static void print_value(const cJSON *item, int depth, char **buf, int *len);

static void ensure_capacity(char **buf, int *len, int needed) {
    while (*len < needed) {
        int new_size = (*len) * 2;
        char *new_buf = (char *)cJSON_malloc(new_size);
        if (new_buf) {
            memcpy(new_buf, *buf, *len);
            cJSON_free(*buf);
            *buf = new_buf;
            *len = new_size;
        }
    }
}

static void append_str(char **buf, int *len, int *pos, const char *s) {
    int slen = strlen(s);
    ensure_capacity(buf, len, *pos + slen + 1);
    memcpy(*buf + *pos, s, slen);
    *pos += slen;
    (*buf)[*pos] = '\0';
}

static void append_char(char **buf, int *len, int *pos, char c) {
    ensure_capacity(buf, len, *pos + 2);
    (*buf)[*pos++] = c;
    (*buf)[*pos] = '\0';
}

static void print_string(const char *str, char **buf, int *len, int *pos) {
    append_char(buf, len, pos, '"');
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  append_str(buf, len, pos, "\\\""); break;
            case '\\': append_str(buf, len, pos, "\\\\"); break;
            case '\b': append_str(buf, len, pos, "\\b"); break;
            case '\f': append_str(buf, len, pos, "\\f"); break;
            case '\n': append_str(buf, len, pos, "\\n"); break;
            case '\r': append_str(buf, len, pos, "\\r"); break;
            case '\t': append_str(buf, len, pos, "\\t"); break;
            default:   append_char(buf, len, pos, *p); break;
        }
    }
    append_char(buf, len, pos, '"');
}

static void print_array(const cJSON *item, int depth, char **buf, int *len, int *pos) {
    append_char(buf, len, pos, '[');
    
    cJSON *child = item->child;
    while (child) {
        if (child->prev) append_char(buf, len, pos, ',');
        print_value(child, depth + 1, buf, len);
        child = child->next;
    }
    
    append_char(buf, len, pos, ']');
}

static void print_object(const cJSON *item, int depth, char **buf, int *len, int *pos) {
    append_char(buf, len, pos, '{');
    
    cJSON *child = item->child;
    while (child) {
        if (child->prev) append_char(buf, len, pos, ',');
        if (child->string) {
            print_string(child->string, buf, len, pos);
        } else {
            print_string("", buf, len, pos);
        }
        append_char(buf, len, pos, ':');
        print_value(child, depth + 1, buf, len);
        child = child->next;
    }
    
    append_char(buf, len, pos, '}');
}

static void print_value(const cJSON *item, int depth, char **buf, int *len) {
    if (!item) return;
    
    switch (item->type & 0xFF) {
        case cJSON_False:
            append_str(buf, len, buf ? &((int){0}) : NULL, "false");
            break;
        case cJSON_True:
            append_str(buf, len, buf ? &((int){0}) : NULL, "true");
            break;
        case cJSON_NULL:
            append_str(buf, len, buf ? &((int){0}) : NULL, "null");
            break;
        case cJSON_Number: {
            char num[64];
            if (item->valuedouble == (double)item->valueint)
                sprintf(num, "%d", item->valueint);
            else
                sprintf(num, "%g", item->valuedouble);
            append_str(buf, len, buf ? &((int){0}) : NULL, num);
            break;
        }
        case cJSON_String:
            print_string(item->valuestring, buf, len, buf ? &((int){0}) : NULL);
            break;
        case cJSON_Array:
            print_array(item, depth, buf, len, buf ? &((int){0}) : NULL);
            break;
        case cJSON_Object:
            print_object(item, depth, buf, len, buf ? &((int){0}) : NULL);
            break;
        default:
            break;
    }
}

char *cJSON_PrintUnformatted(const cJSON *item) {
    if (!item) return NULL;
    
    int buf_len = 256;
    char *buf = (char *)cJSON_malloc(buf_len);
    if (!buf) return NULL;
    buf[0] = '\0';
    int pos = 0;
    
    print_value(item, 0, &buf, &pos);
    
    return buf;
}

char *cJSON_Print(const cJSON *item) {
    // For simplicity, just use unformatted
    return cJSON_PrintUnformatted(item);
}

/* ─── Accessors ───────────────────────────────────────────────── */

cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string) {
    if (!object || !string) return NULL;
    
    cJSON *child = object->child;
    while (child) {
        if (child->string && cJSON_strcasecmp(string, child->string) == 0)
            return child;
        child = child->next;
    }
    return NULL;
}

cJSON *cJSON_GetArrayItem(const cJSON * const array, int index) {
    if (!array || index < 0) return NULL;
    
    cJSON *child = array->child;
    for (int i = 0; i < index && child; i++) {
        child = child->next;
    }
    return child;
}

int cJSON_GetArraySize(const cJSON * const array) {
    if (!array) return 0;
    
    int count = 0;
    cJSON *child = array->child;
    while (child) {
        count++;
        child = child->next;
    }
    return count;
}

/* ─── Cleanup ─────────────────────────────────────────────────── */

void cJSON_Delete(cJSON *c) {
    if (!c) return;
    
    cJSON_Delete(c->child);
    cJSON_Delete(c->next);
    
    if (c->string) cJSON_free(c->string);
    if (c->valuestring && c->type == cJSON_String) cJSON_free(c->valuestring);
    cJSON_free(c);
}

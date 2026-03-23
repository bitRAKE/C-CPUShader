#include "shader_variables.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ascii_space(const char *text)
{
    while (text != NULL &&
           (*text == ' ' ||
            *text == '\t' ||
            *text == '\r' ||
            *text == '\n' ||
            *text == '\f' ||
            *text == '\v'))
    {
        text++;
    }

    return text;
}

static bool parse_trimmed_float(const char *text, float *value_out)
{
    const char *trimmed = skip_ascii_space(text);
    char *end = NULL;
    float value;

    if (trimmed == NULL || trimmed[0] == '\0' || value_out == NULL) {
        return false;
    }

    value = strtof(trimmed, &end);
    if (end == trimmed || skip_ascii_space(end)[0] != '\0') {
        return false;
    }

    *value_out = value;
    return true;
}

static bool parse_trimmed_int(const char *text, int *value_out)
{
    const char *trimmed = skip_ascii_space(text);
    char *end = NULL;
    long value;

    if (trimmed == NULL || trimmed[0] == '\0' || value_out == NULL) {
        return false;
    }

    value = strtol(trimmed, &end, 10);
    if (end == trimmed || skip_ascii_space(end)[0] != '\0' || value < INT_MIN || value > INT_MAX) {
        return false;
    }

    *value_out = (int)value;
    return true;
}

static bool parse_trimmed_bool(const char *text, bool *value_out)
{
    const char *trimmed = skip_ascii_space(text);
    size_t length;
    char normalized[16];

    if (trimmed == NULL || trimmed[0] == '\0' || value_out == NULL) {
        return false;
    }

    length = strlen(trimmed);
    while (length > 0 &&
           (trimmed[length - 1] == ' ' ||
            trimmed[length - 1] == '\t' ||
            trimmed[length - 1] == '\r' ||
            trimmed[length - 1] == '\n' ||
            trimmed[length - 1] == '\f' ||
            trimmed[length - 1] == '\v'))
    {
        length--;
    }

    if (length == 0 || length >= sizeof(normalized)) {
        return false;
    }

    for (size_t index = 0; index < length; index++) {
        char ch = trimmed[index];

        if (ch >= 'A' && ch <= 'Z') {
            ch = (char)(ch - 'A' + 'a');
        }
        normalized[index] = ch;
    }
    normalized[length] = '\0';

    if (strcmp(normalized, "1") == 0 || strcmp(normalized, "true") == 0) {
        *value_out = true;
        return true;
    }
    if (strcmp(normalized, "0") == 0 || strcmp(normalized, "false") == 0) {
        *value_out = false;
        return true;
    }

    return false;
}

static bool parse_float_components(const char *text, int component_count, float *components_out)
{
    const char *cursor = text;

    if (text == NULL || components_out == NULL || component_count <= 0) {
        return false;
    }

    for (int index = 0; index < component_count; index++) {
        char *end = NULL;

        cursor = skip_ascii_space(cursor);
        if (cursor == NULL || cursor[0] == '\0') {
            return false;
        }

        components_out[index] = strtof(cursor, &end);
        if (end == cursor) {
            return false;
        }

        cursor = skip_ascii_space(end);
        if (index + 1 < component_count) {
            if (cursor == NULL || cursor[0] != ',') {
                return false;
            }
            cursor++;
        }
    }

    cursor = skip_ascii_space(cursor);
    return cursor != NULL && cursor[0] == '\0';
}

size_t shader_variable_type_size(shader_variable_type_t type)
{
    switch (type) {
        case SHADER_VARIABLE_TYPE_FLOAT:
            return sizeof(float);

        case SHADER_VARIABLE_TYPE_INT:
            return sizeof(int);

        case SHADER_VARIABLE_TYPE_BOOL:
            return sizeof(bool);

        case SHADER_VARIABLE_TYPE_VEC2:
            return sizeof(vec2_t);

        case SHADER_VARIABLE_TYPE_VEC3:
            return sizeof(vec3_t);

        case SHADER_VARIABLE_TYPE_VEC4:
            return sizeof(vec4_t);
    }

    return 0;
}

const char *shader_variable_type_name(shader_variable_type_t type)
{
    switch (type) {
        case SHADER_VARIABLE_TYPE_FLOAT:
            return "float";

        case SHADER_VARIABLE_TYPE_INT:
            return "int";

        case SHADER_VARIABLE_TYPE_BOOL:
            return "bool";

        case SHADER_VARIABLE_TYPE_VEC2:
            return "vec2_t";

        case SHADER_VARIABLE_TYPE_VEC3:
            return "vec3_t";

        case SHADER_VARIABLE_TYPE_VEC4:
            return "vec4_t";
    }

    return "unknown";
}

bool shader_variable_parse_value(shader_variable_type_t type, const char *text, void *dest, char *error_text, size_t error_text_size)
{
    float components[4];

    if (dest == NULL) {
        if (error_text != NULL && error_text_size > 0) {
            snprintf(error_text, error_text_size, "Variable destination is null.");
        }
        return false;
    }

    switch (type) {
        case SHADER_VARIABLE_TYPE_FLOAT:
            if (parse_trimmed_float(text, (float *)dest)) {
                return true;
            }
            break;

        case SHADER_VARIABLE_TYPE_INT:
            if (parse_trimmed_int(text, (int *)dest)) {
                return true;
            }
            break;

        case SHADER_VARIABLE_TYPE_BOOL:
            if (parse_trimmed_bool(text, (bool *)dest)) {
                return true;
            }
            break;

        case SHADER_VARIABLE_TYPE_VEC2:
            if (parse_float_components(text, 2, components)) {
                *(vec2_t *)dest = vec2(components[0], components[1]);
                return true;
            }
            break;

        case SHADER_VARIABLE_TYPE_VEC3:
            if (parse_float_components(text, 3, components)) {
                *(vec3_t *)dest = vec3(components[0], components[1], components[2]);
                return true;
            }
            break;

        case SHADER_VARIABLE_TYPE_VEC4:
            if (parse_float_components(text, 4, components)) {
                *(vec4_t *)dest = vec4(components[0], components[1], components[2], components[3]);
                return true;
            }
            break;
    }

    if (error_text != NULL && error_text_size > 0) {
        snprintf(error_text, error_text_size, "Expected a %s value.", shader_variable_type_name(type));
    }
    return false;
}

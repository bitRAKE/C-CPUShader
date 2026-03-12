#pragma once

#include "defines.h"

static inline bool u_key_down(const shader_uniforms_t *uniforms, uint virtual_key)
{
    uint word_index;
    uint bit_index;

    if (uniforms == NULL || virtual_key >= 256u) {
        return false;
    }

    word_index = virtual_key >> 5;
    bit_index = virtual_key & 31u;
    return (uniforms->keys.words[word_index] & (1u << bit_index)) != 0;
}

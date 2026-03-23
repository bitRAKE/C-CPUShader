/*
 * shader_defines.h -- Compatibility wrapper.
 *
 * Allows shader sources to include "shader_defines.h" uniformly.
 * In the host build (-Isrc), this resolves here and forwards to defines.h
 * (which transitively includes sdk/shader_defines.h).
 * In the plugin build (-Isdk), it resolves to sdk/shader_defines.h directly.
 */

#pragma once

#include "defines.h"

#pragma once

/* Scan for plugin DLLs. Searches {exe}/plugins/ by default,
 * plus any additional directories specified via --plugin-dir.
 * All directories are scanned recursively up to MAX_SCAN_DEPTH.
 * Discovers collections, checks ABI versions, and merges
 * shaders into the dynamic catalog.
 * Returns the number of plugins successfully loaded.
 * Call catalog_init() before this function. */
int plugin_loader_scan(const char **extra_dirs, int extra_dir_count);

/* Release all loaded plugin DLLs. Call at shutdown. */
void plugin_loader_cleanup(void);

/* Diagnostic messages from the last scan. */
int         plugin_loader_diagnostic_count(void);
const char *plugin_loader_diagnostic(int index);

#ifndef UB_CONFIG_IO_H
#define UB_CONFIG_IO_H

#include "config.h"

/* Loads %LOCALAPPDATA%\UnderBuffer\config.ini, creating it with a
 * commented default template if missing. Always leaves cfg in a
 * valid state (falls back to defaults on any I/O failure). */
void ub_config_load(ub_config_t *cfg);

#endif

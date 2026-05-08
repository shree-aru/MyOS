/* ============================================================================
 * MyOS - Custom Native Forth Compiler & Scripting Engine Header
 * ============================================================================ */

#ifndef COMPILER_H
#define COMPILER_H

#include "kernel.h"

void forth_init(void);
void forth_run_command(const char* line);
void forth_shell(void);

#endif /* COMPILER_H */

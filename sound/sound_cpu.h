#pragma once
#include <stdint.h>
#include "../board/zr107.h"
#ifdef __cplusplus
extern "C" {
#endif
void zrsound_reset(ZRBoard *board);
int zrsound_run(ZRBoard *board, unsigned instructions);
uint32_t zrsound_pc(void);
#ifdef __cplusplus
}
#endif

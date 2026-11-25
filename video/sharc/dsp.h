#ifndef MIDNRUN_NATIVE_DSP_H
#define MIDNRUN_NATIVE_DSP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct ZRBoard;
typedef struct ZRDSP ZRDSP;
ZRDSP *zr_dsp_create(struct ZRBoard *board);
void zr_dsp_destroy(ZRDSP *);
/* Run at most 'budget' translated instructions. Zero includes idle/reset.
 * Negative reports an unsupported firmware operation or bus access. */
int zr_dsp_run(ZRDSP *, unsigned budget);
const char *zr_dsp_error(const ZRDSP *);
uint32_t zr_dsp_pc(const ZRDSP *);
#ifdef __cplusplus
}
#endif
#endif

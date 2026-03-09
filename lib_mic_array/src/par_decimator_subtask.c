#include <xs1.h>
#include <platform.h>
#include <stdint.h>

#include "xmath/xmath.h"
#include "xcore/parallel.h"
#include "mic_array/etc/fir_1x16_bit.h"

DECLARE_JOB(par_decimator_subtask, (int32_t*, uint32_t*, const uint32_t*) );

void par_decimator_subtask_run(
        int32_t *sample_out,
        uint32_t *hist0,
        uint32_t *hist1,
        const uint32_t* s1_filter_coef)
{
    PAR_JOBS (
        PJOB(par_decimator_subtask, (&sample_out[0], hist0, s1_filter_coef)),
        PJOB(par_decimator_subtask, (&sample_out[1], hist1, s1_filter_coef))
    );
    //sample_out[0] = fir_1x16_bit(hist0, s1_filter_coef);
    //sample_out[1] = fir_1x16_bit(hist1, s1_filter_coef);
}

void par_decimator_subtask(int32_t* sample_out,
  uint32_t* hist,
  const uint32_t *s1_filter_coef )
{
    *sample_out = fir_1x16_bit(hist, s1_filter_coef);
}

#include <xs1.h>
#include <platform.h>
#include <stdint.h>

#include "xmath/xmath.h"
#include "xcore/parallel.h"
#include "xcore/chanend.h"
#include "mic_array/etc/fir_1x16_bit.h"
#include <xcore/select.h>

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

void decimator_1st_stage_1_sample(chanend_t c_decimator, uint32_t *hist, const uint32_t* s1_filter_coef)
{
    SELECT_RES(
        CASE_THEN(c_decimator, event_new_sample)
    )
    {
        event_new_sample:
        {
            hist[0] = chanend_in_word(c_decimator);
            int32_t sample_out = fir_1x16_bit(hist, s1_filter_coef);
            chanend_out_word(c_decimator, sample_out);
            continue;
        }
    }

}

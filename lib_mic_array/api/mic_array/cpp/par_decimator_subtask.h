#pragma once

#include <stdint.h>
#include "mic_array/etc/fir_1x16_bit.h"

extern "C" void par_decimator_subtask_run(
        int32_t *sample_out,
        uint32_t *hist0,
        uint32_t *hist1,
        const uint32_t* s1_filter_coef);
// Copyright 2022-2026 XMOS LIMITED.
// This Software is subject to the terms of the XMOS Public Licence: Version 1.

#pragma once

#include <cstdint>
#include <string>
#include <cassert>

#include "xmath/xmath.h"
#include "mic_array/etc/fir_1x16_bit.h"
#include "par_decimator_subtask.h"

// This has caused problems previously, so just catch the problems here.
#if defined (MIC_COUNT)
# error Application must not define the following as precompiler macros: MIC_COUNT, S2_DEC_FACTOR.
#endif


namespace  mic_array {

/**
 * @brief Rotate 8-word buffer 1 word up.
 *
 * Each word `buff[k]` is moved to `buff[(k+1)%8]`.
 *
 * @param buff  Word buffer to be rotated.
 */
static inline
void shift_buffer(uint32_t* buff);

static inline
void shift_by_1_and_store_not_inplace(uint32_t* buff_src, uint32_t *buff_dst);

static inline
void shift_by_2_and_store_inplace(uint32_t* buff_src);

/**
 * @brief First and Second Stage Decimator
 *
 * This class template represents a two stage decimator which converts a stream
 * of PDM samples to a lower sample rate stream of PCM samples.
 *
 * Concrete implementations of this class template are meant to be used as the
 * `TDecimator` template parameter in the @ref MicArray class template.
 *
 * @tparam MIC_COUNT      Number of microphone channels.
 */
template <unsigned MIC_COUNT>
class TwoStageDecimator
{
  private:

    /**
     * Stage 1 decimator configuration and state.
     */
    struct {
      /**
       * Pointer to filter coefficients for Stage 1
       */

      const uint32_t* filter_coef;
      /**
       * Pointer to filter state (PDM history) for stage-1 filter.
       */
      uint32_t *pdm_history_ptr;

      uint32_t *pdm_history_ptr0;
      uint32_t *pdm_history_ptr1;

      /**
       * Per-mic channel filter state (PDM history) size in 32-bit words for stage-1 filter.
       */
      unsigned pdm_history_sz;
    } stage1;

    /**
     * Stage 2 decimation configuration and state.
     */
    struct {
      /**
       * Stage 2 FIR filters
       */
      filter_fir_s32_t filters[MIC_COUNT];
      /**
       * Stage 2 filter decimation factor.
       */
      unsigned decimation_factor;
    } stage2;

  public:

    constexpr TwoStageDecimator() noexcept { }

    /**
     * @brief Initialize the two-stage decimator from a configuration struct
     * @ref mic_array_decimator_conf_t @p decimator_conf
     *
     * Reads stage-1 and stage-2 filter parameters from @p decimator_conf and prepares
     * internal state:
     * The caller must ensure all pointers inside @p decimator_conf.filter_conf[0]
     * and @p decimator_conf.filter_conf[1] are valid and persist for the
     * lifetime of the decimator.
     *
     * @param decimator_conf Decimator pipeline configuration.
     */
    void Init(mic_array_decimator_conf_t &decimator_conf);

    /**
     * @brief Process one block of PDM data.
     *
     * Processes a block of PDM data to produce an output sample from the
     * second stage decimator.
     *
     * `pdm_block` contains exactly enough PDM samples to produce a single
     * output sample from the second stage decimator. The layout of `pdm_block`
     * should (effectively) be:
     *
     * @code{.cpp}
     *  struct {
     *    struct {
     *      // lower word indices are older samples.
     *      // less significant bits in a word are older samples.
     *      uint32_t samples[S2_DEC_FACTOR];
     *    } microphone[MIC_COUNT]; // mic channels are in ascending order
     *  } pdm_block;
     * @endcode
     *
     * A single output sample from the second stage decimator is computed and
     * written to `sample_out[]`.
     *
     * @param sample_out  Output sample vector.
     * @param pdm_block   PDM data to be processed.
     */
    void ProcessBlock(
        int32_t sample_out[MIC_COUNT],
        uint32_t *pdm_block);
  };
}

//////////////////////////////////////////////
// Template function implementations below. //
//////////////////////////////////////////////

template <unsigned MIC_COUNT>
void mic_array::TwoStageDecimator<MIC_COUNT>::Init(
    mic_array_decimator_conf_t &decimator_conf)
{
  this->stage1.filter_coef = (const uint32_t*)decimator_conf.filter_conf[0].coef;
  this->stage1.pdm_history_ptr = (uint32_t*)decimator_conf.filter_conf[0].state;

  this->stage1.pdm_history_ptr0 = (uint32_t*)decimator_conf.filter_conf[0].state0;
  this->stage1.pdm_history_ptr1 = (uint32_t*)decimator_conf.filter_conf[0].state1;

  this->stage1.pdm_history_sz = decimator_conf.filter_conf[0].state_words_per_channel;

  memset(this->stage1.pdm_history_ptr, 0x55, sizeof(int32_t) * MIC_COUNT * this->stage1.pdm_history_sz);

  for(int k = 0; k < MIC_COUNT; k++){
    filter_fir_s32_init(&this->stage2.filters[k], decimator_conf.filter_conf[1].state + (k * decimator_conf.filter_conf[1].state_words_per_channel),
                        decimator_conf.filter_conf[1].num_taps, decimator_conf.filter_conf[1].coef, decimator_conf.filter_conf[1].shr);
  }
  this->stage2.decimation_factor = decimator_conf.filter_conf[1].decimation_factor;
}

#if 0
template <unsigned MIC_COUNT>
void mic_array::TwoStageDecimator<MIC_COUNT>
    ::ProcessBlock(
        int32_t sample_out[1],
        uint32_t *pdm_block)
{
  uint32_t* hist0 = this->stage1.pdm_history_ptr0;
  uint32_t* hist1 = this->stage1.pdm_history_ptr1;

  hist0[0] = pdm_block[0];

  hist1[1] = pdm_block[0];
  hist1[0] = pdm_block[1];
  int32_t streamA_sample0 = fir_1x16_bit(hist0, this->stage1.filter_coef);
  int32_t streamA_sample1 = fir_1x16_bit(hist1, this->stage1.filter_coef);
  shift_by_1_and_store_not_inplace(hist1, hist0);
  shift_by_2_and_store_inplace(hist1);
  filter_fir_s32_add_sample(&this->stage2.filters[0], streamA_sample0);
  sample_out[0] = filter_fir_s32(&this->stage2.filters[0], streamA_sample1);
}
#endif


template <unsigned MIC_COUNT>
void mic_array::TwoStageDecimator<MIC_COUNT>
    ::ProcessBlock(
        int32_t sample_out[1],
        uint32_t *pdm_block)
{
  uint32_t* hist0 = this->stage1.pdm_history_ptr0;
  uint32_t* hist1 = this->stage1.pdm_history_ptr1;

  hist0[0] = pdm_block[0];

  hist1[1] = pdm_block[0];
  hist1[0] = pdm_block[1];

  int32_t output_samples[2];
  par_decimator_subtask_run(output_samples, hist0, hist1, this->stage1.filter_coef);

  shift_by_1_and_store_not_inplace(hist1, hist0);
  shift_by_2_and_store_inplace(hist1);
  filter_fir_s32_add_sample(&this->stage2.filters[0], output_samples[0]);
  sample_out[0] = filter_fir_s32(&this->stage2.filters[0], output_samples[1]);
}

static inline
void mic_array::shift_buffer(uint32_t* buff)
{
  #if defined(__XS3A__)
  uint32_t* src = &buff[-1];
  asm volatile("vldd %0[0]; vstd %1[0];" :: "r"(src), "r"(buff) : "memory" );
  #elif defined(__VX4B__)
  uint32_t* src = &buff[-1];
  asm volatile("xm.vldd %0; xm.vstd %1;" :: "r"(src), "r"(buff) : "memory" );
  #else // C fallback
  for (unsigned k = 7; k > 0; k--) {
    buff[k] = buff[k-1];
  }
  #endif
}

static inline
void mic_array::shift_by_1_and_store_not_inplace(uint32_t* buff_src, uint32_t *buff_dst)
{
  #if defined(__XS3A__)
  uint32_t* src = &buff_src[-1];
  asm volatile("vldd %0[0]; vstd %1[0];" :: "r"(src), "r"(buff_dst) : "memory" );
  #elif defined(__VX4B__)
  uint32_t* src = &buff_src[-1];
  asm volatile("xm.vldd %0; xm.vstd %1;" :: "r"(src), "r"(buff_dst) : "memory" );
  #else // C fallback
  for (unsigned k = 7; k > 0; k--) {
    buff[k] = buff_dst[k-1];
  }
  #endif
}

static inline
void mic_array::shift_by_2_and_store_inplace(uint32_t* buff_src)
{
  #if defined(__XS3A__)
  uint32_t* src = &buff_src[-2];
  asm volatile("vldd %0[0]; vstd %1[0];" :: "r"(src), "r"(buff_src) : "memory" );
  #elif defined(__VX4B__)
  uint32_t* src = &buff_src[-2];
  asm volatile("xm.vldd %0; xm.vstd %1;" :: "r"(src), "r"(buff_src) : "memory" );
  #else // C fallback
  for (unsigned k = 7; k > 0; k--) {
    buff[k] = buff_src[k-2];
  }
  #endif
}

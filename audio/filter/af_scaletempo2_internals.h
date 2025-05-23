// This filter was ported from Chromium
// (https://chromium.googlesource.com/chromium/chromium/+/51ed77e3f37a9a9b80d6d0a8259e84a8ca635259/media/filters/audio_renderer_algorithm.cc)
//
// Copyright 2015 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "common/common.h"

struct mp_scaletempo2_opts {
    // Max/min supported playback rates for fast/slow audio. Audio outside of these
    // ranges are muted.
    // Audio at these speeds would sound better under a frequency domain algorithm.
    float min_playback_rate;
    float max_playback_rate;
    // Overlap-and-add window size in milliseconds.
    float ola_window_size_ms;
    // Size of search interval in milliseconds. The search interval is
    // [-delta delta] around |output_index| * |playback_rate|. So the search
    // interval is 2 * delta.
    float wsola_search_interval_ms;
};

struct mp_scaletempo2 {
    struct mp_scaletempo2_opts *opts;
    // Number of channels in audio stream.
    int channels;
    // Sample rate of audio stream.
    int samples_per_second;
    // If muted, keep track of partial frames that should have been skipped over.
    double muted_partial_frame;
    // Book keeping of the current time of generated audio, in frames.
    // Corresponds to the center of |search_block|. This is increased in
    // intervals of |ola_hop_size| multiplied by the current playback_rate,
    // for every WSOLA iteration. This tracks the number of advanced frames as
    // a double to achieve accurate playback rates beyond the integer precision
    // of |search_block_index|.
    // Needs to be adjusted like any other index when frames are evicted from
    // |input_buffer|.
    double output_time;
    // The offset of the center frame of |search_block| w.r.t. its first frame.
    int search_block_center_offset;
    // Index of the beginning of the |search_block|, in frames. This may be
    // negative, which is handled by |peek_audio_with_zero_prepend|.
    int search_block_index;
    // Number of Blocks to search to find the most similar one to the target
    // frame.
    int num_candidate_blocks;
    // Index of the beginning of the target block, counted in frames.
    int target_block_index;
    // Overlap-and-add window size in frames.
    int ola_window_size;
    // The hop size of overlap-and-add in frames. This implementation assumes 50%
    // overlap-and-add.
    int ola_hop_size;
    // Number of frames in |wsola_output| that overlap-and-add is completed for
    // them and can be copied to output if fill_buffer() is called. It also
    // specifies the index where the next WSOLA window has to overlap-and-add.
    int num_complete_frames;
    // Whether |wsola_output| contains an additional |ola_hop_size| of overlap
    // frames for the next iteration.
    bool wsola_output_started;
    // Overlap-and-add window.
    float *ola_window;
    // Transition window, used to update |optimal_block| by a weighted sum of
    // |optimal_block| and |target_block|.
    float *transition_window;
    // This stores a part of the output that is created but couldn't be rendered.
    // Output is generated frame-by-frame which at some point might exceed the
    // number of requested samples. Furthermore, due to overlap-and-add,
    // the last half-window of the output is incomplete, which is stored in this
    // buffer.
    float **wsola_output;
    int wsola_output_size;
    // Auxiliary variables to avoid allocation in every iteration.
    // Stores the optimal block in every iteration. This is the most
    // similar block to |target_block| within |search_block| and it is
    // overlap-and-added to |wsola_output|.
    float **optimal_block;
    // A block of data that search is performed over to find the |optimal_block|.
    float **search_block;
    int search_block_size;
    // Stores the target block, denoted as |target| above. |search_block| is
    // searched for a block (|optimal_block|) that is most similar to
    // |target_block|.
    float **target_block;
    // Buffered audio data.
    float **input_buffer;
    int input_buffer_frames;
    // How many frames in |input_buffer| need to be flushed by padding with
    // silence to process the final packet. While this is nonzero, the filter
    // appends silence to |input_buffer| until these frames are processed.
    int input_buffer_final_frames;
    // How many additional frames of silence have been added to |input_buffer|
    // for padding after the final packet.
    int input_buffer_added_silence;
    float *energy_candidate_blocks;
};

void mp_scaletempo2_destroy(struct mp_scaletempo2 *p);
void mp_scaletempo2_reset(struct mp_scaletempo2 *p);
void mp_scaletempo2_init(struct mp_scaletempo2 *p, int channels, int rate);
double mp_scaletempo2_get_latency(struct mp_scaletempo2 *p, double playback_rate);
int mp_scaletempo2_fill_input_buffer(struct mp_scaletempo2 *p,
    uint8_t **planes, int frame_size, double playback_rate);
void mp_scaletempo2_set_final(struct mp_scaletempo2 *p);
int mp_scaletempo2_fill_buffer(struct mp_scaletempo2 *p,
    float **dest, int dest_size, double playback_rate);
bool mp_scaletempo2_frames_available(struct mp_scaletempo2 *p, double playback_rate);

#include <float.h>
#include <math.h>

#include "audio/chmap.h"
#include "audio/filter/af_scaletempo2_internals.h"

#include "config.h"

// Algorithm overview (from chromium):
// Waveform Similarity Overlap-and-add (WSOLA).
//
// One WSOLA iteration
//
// 1) Extract |target_block| as input frames at indices
//    [|target_block_index|, |target_block_index| + |ola_window_size|).
//    Note that |target_block| is the "natural" continuation of the output.
//
// 2) Extract |search_block| as input frames at indices
//    [|search_block_index|,
//     |search_block_index| + |num_candidate_blocks| + |ola_window_size|).
//
// 3) Find a block within the |search_block| that is most similar
//    to |target_block|. Let |optimal_index| be the index of such block and
//    write it to |optimal_block|.
//
// 4) Update:
//    |optimal_block| = |transition_window| * |target_block| +
//    (1 - |transition_window|) * |optimal_block|.
//
// 5) Overlap-and-add |optimal_block| to the |wsola_output|.
//
// 6) Update:write

struct interval {
    int lo;
    int hi;
};

static bool in_interval(int n, struct interval q)
{
    return n >= q.lo && n <= q.hi;
}

static float **realloc_2d(float **p, int x, int y)
{
    float **array = realloc(p, sizeof(float*) * x + sizeof(float) * x * y);
    float* data = (float*) (array + x);
    for (int i = 0; i < x; ++i) {
        array[i] = data + i * y;
    }
    return array;
}

static void zero_2d(float **a, int x, int y)
{
    memset(a + x, 0, sizeof(float) * x * y);
}

static void zero_2d_partial(float **a, int x, int y)
{
    for (int i = 0; i < x; ++i) {
        memset(a[i], 0, sizeof(float) * y);
    }
}

// Energies of sliding windows of channels are interleaved.
// The number windows is |input_frames| - (|frames_per_window| - 1), hence,
// the method assumes |energy| must be, at least, of size
// (|input_frames| - (|frames_per_window| - 1)) * |channels|.
static void multi_channel_moving_block_energies(
    float **input, int input_frames, int channels,
    int frames_per_block, float *energy)
{
    int num_blocks = input_frames - (frames_per_block - 1);

    for (int k = 0; k < channels; ++k) {
        const float* input_channel = input[k];

        energy[k] = 0;

        // First block of channel |k|.
        for (int m = 0; m < frames_per_block; ++m) {
            energy[k] += input_channel[m] * input_channel[m];
        }

        const float* slide_out = input_channel;
        const float* slide_in = input_channel + frames_per_block;
        for (int n = 1; n < num_blocks; ++n, ++slide_in, ++slide_out) {
            energy[k + n * channels] = energy[k + (n - 1) * channels]
                - *slide_out * *slide_out + *slide_in * *slide_in;
        }
    }
}

static float multi_channel_similarity_measure(
    const float* dot_prod,
    const float* energy_target, const float* energy_candidate,
    int channels)
{
    const float epsilon = 1e-12f;
    float similarity_measure = 0.0f;
    for (int n = 0; n < channels; ++n) {
        similarity_measure += dot_prod[n] * energy_target[n]
            / sqrtf(energy_target[n] * energy_candidate[n] + epsilon);
    }
    return similarity_measure;
}

#if HAVE_VECTOR

typedef float v8sf __attribute__ ((vector_size (32), aligned (1)));

// Dot-product of channels of two AudioBus. For each AudioBus an offset is
// given. |dot_product[k]| is the dot-product of channel |k|. The caller should
// allocate sufficient space for |dot_product|.
static void multi_channel_dot_product(
    float **a, int frame_offset_a,
    float **b, int frame_offset_b,
    int channels,
    int num_frames, float *dot_product)
{
    assert(frame_offset_a >= 0);
    assert(frame_offset_b >= 0);

    for (int k = 0; k < channels; ++k) {
        const float* ch_a = a[k] + frame_offset_a;
        const float* ch_b = b[k] + frame_offset_b;
        float sum = 0.0;
        if (num_frames < 32)
            goto rest;

        const v8sf *va = (const v8sf *) ch_a;
        const v8sf *vb = (const v8sf *) ch_b;
        v8sf vsum[4] = {
            // Initialize to product of first 32 floats
            va[0] * vb[0],
            va[1] * vb[1],
            va[2] * vb[2],
            va[3] * vb[3],
        };
        va += 4;
        vb += 4;

        // Process `va` and `vb` across four vertical stripes
        for (int n = 1; n < num_frames / 32; n++) {
            vsum[0] += va[0] * vb[0];
            vsum[1] += va[1] * vb[1];
            vsum[2] += va[2] * vb[2];
            vsum[3] += va[3] * vb[3];
            va += 4;
            vb += 4;
        }

        // Vertical sum across `vsum` entries
        vsum[0] += vsum[1];
        vsum[2] += vsum[3];
        vsum[0] += vsum[2];

        // Horizontal sum across `vsum[0]`, could probably be done better but
        // this section is not super performance critical
        float *vf = (float *) &vsum[0];
        sum = vf[0] + vf[1] + vf[2] + vf[3] + vf[4] + vf[5] + vf[6] + vf[7];
        ch_a = (const float *) va;
        ch_b = (const float *) vb;

rest:
        // Process the remainder
        for (int n = 0; n < num_frames % 32; n++)
            sum += *ch_a++ * *ch_b++;

        dot_product[k] = sum;
    }
}

#else // !HAVE_VECTOR

static void multi_channel_dot_product(
    float **a, int frame_offset_a,
    float **b, int frame_offset_b,
    int channels,
    int num_frames, float *dot_product)
{
    assert(frame_offset_a >= 0);
    assert(frame_offset_b >= 0);

    for (int k = 0; k < channels; ++k) {
        const float* ch_a = a[k] + frame_offset_a;
        const float* ch_b = b[k] + frame_offset_b;
        float sum = 0.0;
        for (int n = 0; n < num_frames; n++)
            sum += *ch_a++ * *ch_b++;
        dot_product[k] = sum;
    }
}

#endif // HAVE_VECTOR

// Fit the curve f(x) = a * x^2 + b * x + c such that
//   f(-1) = y[0]
//   f(0) = y[1]
//   f(1) = y[2]
// and return the maximum, assuming that y[0] <= y[1] >= y[2].
static void quadratic_interpolation(
    const float* y_values, float* extremum, float* extremum_value)
{
    float a = 0.5f * (y_values[2] + y_values[0]) - y_values[1];
    float b = 0.5f * (y_values[2] - y_values[0]);
    float c = y_values[1];

    if (a == 0.f) {
        // The coordinates are colinear (within floating-point error).
        *extremum = 0;
        *extremum_value = y_values[1];
    } else {
        *extremum = -b / (2.f * a);
        *extremum_value = a * (*extremum) * (*extremum) + b * (*extremum) + c;
    }
}

// Search a subset of all candid blocks. The search is performed every
// |decimation| frames. This reduces complexity by a factor of about
// 1 / |decimation|. A cubic interpolation is used to have a better estimate of
// the best match.
static int decimated_search(
    int decimation, struct interval exclude_interval,
    float **target_block, int target_block_frames,
    float **search_segment, int search_segment_frames,
    int channels,
    const float *energy_target_block, const float *energy_candidate_blocks)
{
    int num_candidate_blocks = search_segment_frames - (target_block_frames - 1);
    float dot_prod [MP_NUM_CHANNELS];
    float similarity[3];  // Three elements for cubic interpolation.

    int n = 0;
    multi_channel_dot_product(
        target_block, 0,
        search_segment, n,
        channels,
        target_block_frames, dot_prod);
    similarity[0] = multi_channel_similarity_measure(
        dot_prod, energy_target_block,
        &energy_candidate_blocks[n * channels], channels);

    // Set the starting point as optimal point.
    float best_similarity = similarity[0];
    int optimal_index = 0;

    n += decimation;
    if (n >= num_candidate_blocks) {
        return 0;
    }

    multi_channel_dot_product(
        target_block, 0,
        search_segment, n,
        channels,
        target_block_frames, dot_prod);
    similarity[1] = multi_channel_similarity_measure(
        dot_prod, energy_target_block,
        &energy_candidate_blocks[n * channels], channels);

    n += decimation;
    if (n >= num_candidate_blocks) {
        // We cannot do any more sampling. Compare these two values and return the
        // optimal index.
        return similarity[1] > similarity[0] ? decimation : 0;
    }

    for (; n < num_candidate_blocks; n += decimation) {
        multi_channel_dot_product(
            target_block, 0,
            search_segment, n,
            channels,
            target_block_frames, dot_prod);

        similarity[2] = multi_channel_similarity_measure(
            dot_prod, energy_target_block,
            &energy_candidate_blocks[n * channels], channels);

        if ((similarity[1] > similarity[0] && similarity[1] >= similarity[2]) ||
            (similarity[1] >= similarity[0] && similarity[1] > similarity[2]))
        {
            // A local maximum is found. Do a cubic interpolation for a better
            // estimate of candidate maximum.
            float normalized_candidate_index;
            float candidate_similarity;
            quadratic_interpolation(similarity, &normalized_candidate_index,
                                    &candidate_similarity);

            int candidate_index = n - decimation
                 + (int)(normalized_candidate_index * decimation +  0.5f);
            if (candidate_similarity > best_similarity
                && !in_interval(candidate_index, exclude_interval)) {
                optimal_index = candidate_index;
                best_similarity = candidate_similarity;
            }
        } else if (n + decimation >= num_candidate_blocks &&
                   similarity[2] > best_similarity &&
                   !in_interval(n, exclude_interval))
        {
            // If this is the end-point and has a better similarity-measure than
            // optimal, then we accept it as optimal point.
            optimal_index = n;
            best_similarity = similarity[2];
        }
        memmove(similarity, &similarity[1], 2 * sizeof(*similarity));
    }
    return optimal_index;
}

// Search [|low_limit|, |high_limit|] of |search_segment| to find a block that
// is most similar to |target_block|. |energy_target_block| is the energy of the
// |target_block|. |energy_candidate_blocks| is the energy of all blocks within
// |search_block|.
static int full_search(
    int low_limit, int high_limit,
    struct interval exclude_interval,
    float **target_block, int target_block_frames,
    float **search_block, int search_block_frames,
    int channels,
    const float* energy_target_block,
    const float* energy_candidate_blocks)
{
    // int block_size = target_block->frames;
    float dot_prod [sizeof(float) * MP_NUM_CHANNELS];

    float best_similarity = -FLT_MAX;//FLT_MIN;
    int optimal_index = 0;

    for (int n = low_limit; n <= high_limit; ++n) {
        if (in_interval(n, exclude_interval)) {
            continue;
        }
        multi_channel_dot_product(target_block, 0, search_block, n, channels,
            target_block_frames, dot_prod);

        float similarity = multi_channel_similarity_measure(
            dot_prod, energy_target_block,
            &energy_candidate_blocks[n * channels], channels);

        if (similarity > best_similarity) {
            best_similarity = similarity;
            optimal_index = n;
        }
    }

    return optimal_index;
}

// Find the index of the block, within |search_block|, that is most similar
// to |target_block|. Obviously, the returned index is w.r.t. |search_block|.
// |exclude_interval| is an interval that is excluded from the search.
static int compute_optimal_index(
    float **search_block, int search_block_frames,
    float **target_block, int target_block_frames,
    float *energy_candidate_blocks,
    int channels,
    struct interval exclude_interval)
{
    int num_candidate_blocks = search_block_frames - (target_block_frames - 1);

    // This is a compromise between complexity reduction and search accuracy. I
    // don't have a proof that down sample of order 5 is optimal.
    // One can compute a decimation factor that minimizes complexity given
    // the size of |search_block| and |target_block|. However, my experiments
    // show the rate of missing the optimal index is significant.
    // This value is chosen heuristically based on experiments.
    const int search_decimation = 5;

    float energy_target_block [MP_NUM_CHANNELS];
    // energy_candidate_blocks must have at least size
    // sizeof(float) * channels * num_candidate_blocks

    // Energy of all candid frames.
    multi_channel_moving_block_energies(
        search_block,
        search_block_frames,
        channels,
        target_block_frames,
        energy_candidate_blocks);

    // Energy of target frame.
    multi_channel_dot_product(
        target_block, 0,
        target_block, 0,
        channels,
        target_block_frames, energy_target_block);

    int optimal_index = decimated_search(
        search_decimation, exclude_interval,
        target_block, target_block_frames,
        search_block, search_block_frames,
        channels,
        energy_target_block,
        energy_candidate_blocks);

    int lim_low = MPMAX(0, optimal_index - search_decimation);
    int lim_high = MPMIN(num_candidate_blocks - 1,
                            optimal_index + search_decimation);
    return full_search(
        lim_low, lim_high, exclude_interval,
        target_block, target_block_frames,
        search_block, search_block_frames,
        channels,
        energy_target_block, energy_candidate_blocks);
}

static void peek_buffer(struct mp_scaletempo2 *p,
    int frames, int read_offset, int write_offset, float **dest)
{
    assert(p->input_buffer_frames >= frames);
    for (int i = 0; i < p->channels; ++i) {
        memcpy(dest[i] + write_offset,
            p->input_buffer[i] + read_offset,
            frames * sizeof(float));
    }
}

static void seek_buffer(struct mp_scaletempo2 *p, int frames)
{
    assert(p->input_buffer_frames >= frames);
    p->input_buffer_frames -= frames;
    if (p->input_buffer_final_frames > 0) {
        p->input_buffer_final_frames = MPMAX(0, p->input_buffer_final_frames - frames);
    }
    for (int i = 0; i < p->channels; ++i) {
        memmove(p->input_buffer[i], p->input_buffer[i] + frames,
            p->input_buffer_frames * sizeof(float));
    }
}

static int write_completed_frames_to(struct mp_scaletempo2 *p,
    int requested_frames, int dest_offset, float **dest)
{
    int rendered_frames = MPMIN(p->num_complete_frames, requested_frames);

    if (rendered_frames == 0)
        return 0;  // There is nothing to read from |wsola_output|, return.

    for (int i = 0; i < p->channels; ++i) {
        memcpy(dest[i] + dest_offset, p->wsola_output[i],
            rendered_frames * sizeof(float));
    }

    // Remove the frames which are read.
    int frames_to_move = p->wsola_output_size - rendered_frames;
    for (int k = 0; k < p->channels; ++k) {
        float *ch = p->wsola_output[k];
        memmove(ch, &ch[rendered_frames], sizeof(*ch) * frames_to_move);
    }
    p->num_complete_frames -= rendered_frames;
    return rendered_frames;
}

// next output_time for the given playback_rate
static double get_updated_time(struct mp_scaletempo2 *p, double playback_rate)
{
    return p->output_time + p->ola_hop_size * playback_rate;
}

// search_block_index for the given output_time
static int get_search_block_index(struct mp_scaletempo2 *p, double output_time)
{
    return (int)(output_time - p->search_block_center_offset + 0.5);
}

// number of frames needed until a wsola iteration can be performed
static int frames_needed(struct mp_scaletempo2 *p, double playback_rate)
{
    int search_block_index =
        get_search_block_index(p, get_updated_time(p, playback_rate));
    return MPMAX(0, MPMAX(
        p->target_block_index + p->ola_window_size - p->input_buffer_frames,
        search_block_index + p->search_block_size - p->input_buffer_frames));
}

static bool can_perform_wsola(struct mp_scaletempo2 *p, double playback_rate)
{
    return frames_needed(p, playback_rate) <= 0;
}

static void grow_input_buffer(struct mp_scaletempo2 *p, int size)
{
    assert(size > p->input_buffer_size);

    p->input_buffer = realloc_2d(p->input_buffer, p->channels, size);

    float *data = (float*) (p->input_buffer + p->channels);
    for (int k = p->channels - 1; k > 0; k--) {
        memmove(data + k * size, data + k * p->input_buffer_size,
            p->input_buffer_frames * sizeof(float));
    }

    p->input_buffer_size = size;
}

// pad end with silence until a wsola iteration can be performed
static void add_input_buffer_final_silence(struct mp_scaletempo2 *p, double playback_rate)
{
    int needed = frames_needed(p, playback_rate);
    if (needed <= 0)
        return; // no silence needed for iteration

    int required_size = needed + p->input_buffer_frames;
    if (required_size > p->input_buffer_size)
        grow_input_buffer(p, required_size);

    for (int i = 0; i < p->channels; ++i) {
        float *ch_input = p->input_buffer[i];
        for (int j = 0; j < needed; ++j) {
            ch_input[p->input_buffer_frames + j] = 0.0f;
        }
    }

    p->input_buffer_added_silence += needed;
    p->input_buffer_frames += needed;
}

void mp_scaletempo2_set_final(struct mp_scaletempo2 *p)
{
    if (p->input_buffer_final_frames <= 0) {
        p->input_buffer_final_frames = p->input_buffer_frames;
    }
}

int mp_scaletempo2_fill_input_buffer(struct mp_scaletempo2 *p,
    uint8_t **planes, int frame_size, double playback_rate)
{
    int needed = frames_needed(p, playback_rate);
    int read = MPMIN(needed, frame_size);
    if (read == 0)
        return 0;

    int required_size = read + p->input_buffer_frames;
    if (required_size > p->input_buffer_size)
        resize_input_buffer(p, required_size);

    for (int i = 0; i < p->channels; ++i) {
        memcpy(p->input_buffer[i] + p->input_buffer_frames,
            planes[i], read * sizeof(float));
    }

    p->input_buffer_frames += read;
    return read;
}

static bool target_is_within_search_region(struct mp_scaletempo2 *p)
{
    return p->target_block_index >= p->search_block_index
        && p->target_block_index + p->ola_window_size
            <= p->search_block_index + p->search_block_size;
}


static void peek_audio_with_zero_prepend(struct mp_scaletempo2 *p,
    int read_offset_frames, float **dest, int dest_frames)
{
    assert(read_offset_frames + dest_frames <= p->input_buffer_frames);

    int write_offset = 0;
    int num_frames_to_read = dest_frames;
    if (read_offset_frames < 0) {
        int num_zero_frames_appended = MPMIN(
            -read_offset_frames, num_frames_to_read);
        read_offset_frames = 0;
        num_frames_to_read -= num_zero_frames_appended;
        write_offset = num_zero_frames_appended;
        zero_2d_partial(dest, p->channels, num_zero_frames_appended);
    }
    peek_buffer(p, num_frames_to_read, read_offset_frames, write_offset, dest);
}

static void get_optimal_block(struct mp_scaletempo2 *p)
{
    int optimal_index = 0;

    // An interval around last optimal block which is excluded from the search.
    // This is to reduce the buzzy sound. The number 160 is rather arbitrary and
    // derived heuristically.
    const int exclude_interval_length_frames = 160;
    if (target_is_within_search_region(p)) {
        optimal_index = p->target_block_index;
        peek_audio_with_zero_prepend(p,
            optimal_index, p->optimal_block, p->ola_window_size);
    } else {
        peek_audio_with_zero_prepend(p,
            p->target_block_index, p->target_block, p->ola_window_size);
        peek_audio_with_zero_prepend(p,
            p->search_block_index, p->search_block, p->search_block_size);
        int last_optimal = p->target_block_index
            - p->ola_hop_size - p->search_block_index;
        struct interval exclude_iterval = {
            .lo = last_optimal - exclude_interval_length_frames / 2,
            .hi = last_optimal + exclude_interval_length_frames / 2
        };

        // |optimal_index| is in frames and it is relative to the beginning of the
        // |search_block|.
        optimal_index = compute_optimal_index(
            p->search_block, p->search_block_size,
            p->target_block, p->ola_window_size,
            p->energy_candidate_blocks,
            p->channels,
            exclude_iterval);

        // Translate |index| w.r.t. the beginning of |audio_buffer| and extract the
        // optimal block.
        optimal_index += p->search_block_index;
        peek_audio_with_zero_prepend(p,
            optimal_index, p->optimal_block, p->ola_window_size);

        // Make a transition from target block to the optimal block if different.
        // Target block has the best continuation to the current output.
        // Optimal block is the most similar block to the target, however, it might
        // introduce some discontinuity when over-lap-added. Therefore, we combine
        // them for a smoother transition. The length of transition window is twice
        // as that of the optimal-block which makes it like a weighting function
        // where target-block has higher weight close to zero (weight of 1 at index
        // 0) and lower weight close the end.
        for (int k = 0; k < p->channels; ++k) {
            float* ch_opt = p->optimal_block[k];
            float* ch_target = p->target_block[k];
            for (int n = 0; n < p->ola_window_size; ++n) {
                ch_opt[n] = ch_opt[n] * p->transition_window[n]
                    + ch_target[n] * p->transition_window[p->ola_window_size + n];
            }
        }
    }

    // Next target is one hop ahead of the current optimal.
    p->target_block_index = optimal_index + p->ola_hop_size;
}

static void set_output_time(struct mp_scaletempo2 *p, double output_time)
{
    p->output_time = output_time;
    p->search_block_index = get_search_block_index(p, output_time);
}

static void remove_old_input_frames(struct mp_scaletempo2 *p)
{
    const int earliest_used_index = MPMIN(
        p->target_block_index, p->search_block_index);
    if (earliest_used_index <= 0)
        return;  // Nothing to remove.

    // Remove frames from input and adjust indices accordingly.
    seek_buffer(p, earliest_used_index);
    p->target_block_index -= earliest_used_index;
    p->output_time -= earliest_used_index;
    p->search_block_index -= earliest_used_index;
}

static bool run_one_wsola_iteration(struct mp_scaletempo2 *p, double playback_rate)
{
    if (!can_perform_wsola(p, playback_rate)) {
        return false;
    }

    set_output_time(p, get_updated_time(p, playback_rate));
    remove_old_input_frames(p);

    assert(p->search_block_index + p->search_block_size <= p->input_buffer_frames);

    get_optimal_block(p);

    // Overlap-and-add.
    for (int k = 0; k < p->channels; ++k) {
        float* ch_opt_frame = p->optimal_block[k];
        float* ch_output = p->wsola_output[k] + p->num_complete_frames;
        if (p->wsola_output_started) {
            for (int n = 0; n < p->ola_hop_size; ++n) {
                ch_output[n] = ch_output[n] * p->ola_window[p->ola_hop_size + n] +
                    ch_opt_frame[n] * p->ola_window[n];
            }

            // Copy the second half to the output.
            memcpy(&ch_output[p->ola_hop_size], &ch_opt_frame[p->ola_hop_size],
                   sizeof(*ch_opt_frame) * p->ola_hop_size);
        } else {
            // No overlap for the first iteration.
            memcpy(ch_output, ch_opt_frame,
                   sizeof(*ch_opt_frame) * p->ola_window_size);
        }
    }

    p->num_complete_frames += p->ola_hop_size;
    p->wsola_output_started = true;
    return true;
}

static int read_input_buffer(struct mp_scaletempo2 *p, int dest_size, float **dest)
{
    int frames_to_copy = MPMIN(dest_size, p->input_buffer_frames - p->target_block_index);

    if (frames_to_copy <= 0)
        return 0; // There is nothing to read from input buffer; return.

    peek_buffer(p, frames_to_copy, p->target_block_index, 0, dest);
    seek_buffer(p, frames_to_copy);
    return frames_to_copy;
}

int mp_scaletempo2_fill_buffer(struct mp_scaletempo2 *p,
    float **dest, int dest_size, double playback_rate)
{
    if (playback_rate == 0) return 0;

    if (p->input_buffer_final_frames > 0) {
        add_input_buffer_final_silence(p, playback_rate);
    }

    // Optimize the muted case to issue a single clear instead of performing
    // the full crossfade and clearing each crossfaded frame.
    if (playback_rate < p->opts->min_playback_rate
        || (playback_rate > p->opts->max_playback_rate
            && p->opts->max_playback_rate > 0))
    {
        int frames_to_render = MPMIN(dest_size,
            (int) (p->input_buffer_frames / playback_rate));

        // Compute accurate number of frames to actually skip in the source data.
        // Includes the leftover partial frame from last request. However, we can
        // only skip over complete frames, so a partial frame may remain for next
        // time.
        p->muted_partial_frame += frames_to_render * playback_rate;
        int seek_frames = (int) (p->muted_partial_frame);
        zero_2d_partial(dest, p->channels, frames_to_render);
        seek_buffer(p, seek_frames);

        // Determine the partial frame that remains to be skipped for next call. If
        // the user switches back to playing, it may be off time by this partial
        // frame, which would be undetectable. If they subsequently switch to
        // another playback rate that mutes, the code will attempt to line up the
        // frames again.
        p->muted_partial_frame -= seek_frames;
        return frames_to_render;
    }

    int slower_step = (int) ceilf(p->ola_window_size * playback_rate);
    int faster_step = (int) ceilf(p->ola_window_size / playback_rate);

    // Optimize the most common |playback_rate| ~= 1 case to use a single copy
    // instead of copying frame by frame.
    if (p->ola_window_size <= faster_step && slower_step >= p->ola_window_size) {

        if (p->wsola_output_started) {
            p->wsola_output_started = false;

            // sync audio precisely again
            set_output_time(p, p->target_block_index);
            remove_old_input_frames(p);
        }

        return read_input_buffer(p, dest_size, dest);
    }

    int rendered_frames = 0;
    do {
        rendered_frames += write_completed_frames_to(p,
            dest_size - rendered_frames, rendered_frames, dest);
    } while (rendered_frames < dest_size
             && run_one_wsola_iteration(p, playback_rate));
    return rendered_frames;
}

double mp_scaletempo2_get_latency(struct mp_scaletempo2 *p, double playback_rate)
{
    return p->input_buffer_frames - p->output_time
        - p->input_buffer_added_silence
        + p->num_complete_frames * playback_rate;
}

bool mp_scaletempo2_frames_available(struct mp_scaletempo2 *p, double playback_rate)
{
    return (p->input_buffer_final_frames > p->target_block_index &&
            p->input_buffer_final_frames > 0)
        || can_perform_wsola(p, playback_rate)
        || p->num_complete_frames > 0;
}

void mp_scaletempo2_destroy(struct mp_scaletempo2 *p)
{
    free(p->ola_window);
    free(p->transition_window);
    free(p->wsola_output);
    free(p->optimal_block);
    free(p->search_block);
    free(p->target_block);
    free(p->input_buffer);
    free(p->energy_candidate_blocks);
}

void mp_scaletempo2_reset(struct mp_scaletempo2 *p)
{
    p->input_buffer_frames = 0;
    p->input_buffer_final_frames = 0;
    p->input_buffer_added_silence = 0;
    p->output_time = 0.0;
    p->search_block_index = 0;
    p->target_block_index = 0;
    // Clear the queue of decoded packets.
    zero_2d(p->wsola_output, p->channels, p->wsola_output_size);
    p->num_complete_frames = 0;
    p->wsola_output_started = false;
}

// Return a "periodic" Hann window. This is the first L samples of an L+1
// Hann window. It is perfect reconstruction for overlap-and-add.
static void get_symmetric_hanning_window(int window_length, float* window)
{
    const float scale = 2.0f * M_PI / window_length;
    for (int n = 0; n < window_length; ++n)
        window[n] = 0.5f * (1.0f - cosf(n * scale));
}


void mp_scaletempo2_init(struct mp_scaletempo2 *p, int channels, int rate)
{
    p->muted_partial_frame = 0;
    p->output_time = 0;
    p->search_block_index = 0;
    p->target_block_index = 0;
    p->num_complete_frames = 0;
    p->wsola_output_started = false;
    p->channels = channels;

    p->samples_per_second = rate;
    p->num_candidate_blocks = (int)(p->opts->wsola_search_interval_ms
        * p->samples_per_second / 1000);
    p->ola_window_size = (int)(p->opts->ola_window_size_ms
        * p->samples_per_second / 1000);
    // Make sure window size in an even number.
    p->ola_window_size += p->ola_window_size & 1;
    p->ola_hop_size = p->ola_window_size / 2;
    // |num_candidate_blocks| / 2 is the offset of the center of the search
    // block to the center of the first (left most) candidate block. The offset
    // of the center of a candidate block to its left most point is
    // |ola_window_size| / 2 - 1. Note that |ola_window_size| is even and in
    // our convention the center belongs to the left half, so we need to subtract
    // one frame to get the correct offset.
    //
    //                             Search Block
    //              <------------------------------------------->
    //
    //   |ola_window_size| / 2 - 1
    //              <----
    //
    //             |num_candidate_blocks| / 2
    //                   <----------------
    //                                 center
    //              X----X----------------X---------------X-----X
    //              <---------->                     <---------->
    //                Candidate      ...               Candidate
    //                   1,          ...         |num_candidate_blocks|
    p->search_block_center_offset = p->num_candidate_blocks / 2
        + (p->ola_window_size / 2 - 1);
    p->ola_window = realloc(p->ola_window, sizeof(float) * p->ola_window_size);
    get_symmetric_hanning_window(p->ola_window_size, p->ola_window);
    p->transition_window = realloc(p->transition_window,
        sizeof(float) * p->ola_window_size * 2);
    get_symmetric_hanning_window(2 * p->ola_window_size, p->transition_window);

    p->wsola_output_size = p->ola_window_size + p->ola_hop_size;
    p->wsola_output = realloc_2d(p->wsola_output, p->channels, p->wsola_output_size);
    // Initialize for overlap-and-add of the first block.
    zero_2d(p->wsola_output, p->channels, p->wsola_output_size);

    // Auxiliary containers.
    p->optimal_block = realloc_2d(p->optimal_block, p->channels, p->ola_window_size);
    p->search_block_size = p->num_candidate_blocks + (p->ola_window_size - 1);
    p->search_block = realloc_2d(p->search_block, p->channels, p->search_block_size);
    p->target_block = realloc_2d(p->target_block, p->channels, p->ola_window_size);

    p->input_buffer_frames = 0;
    p->input_buffer_final_frames = 0;
    p->input_buffer_added_silence = 0;
    p->input_buffer_size = 4 * MPMAX(p->ola_window_size, p->search_block_size);
    p->input_buffer = realloc_2d(p->input_buffer, channels, p->input_buffer_size);

    p->energy_candidate_blocks = realloc(p->energy_candidate_blocks,
        sizeof(float) * p->channels * p->num_candidate_blocks);
}

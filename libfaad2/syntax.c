/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2004 M. Bakker, Ahead Software AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Ahead Software through Mpeg4AAClicense@nero.com.
**
** $Id: syntax.c,v 1.82 2004/09/04 14:56:29 menno Exp $
**/

/*
   Reads the AAC bitstream as defined in 14496-3 (MPEG-4 Audio)
*/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <string.h>

#include "decoder.h"
#include "syntax.h"
#include "specrec.h"
#include "huffman.h"
#include "bits.h"
#include "pulse.h"
#include "analysis.h"
#include "drc.h"
#ifdef ERROR_RESILIENCE
#include "rvlc.h"
#endif
#ifdef SBR_DEC
#include "sbr_syntax.h"
#endif


/* static function declarations */
static void decode_sce_lfe(NeAACDecHandle hDecoder, NeAACDecFrameInfo *hInfo, bitfile *ld,
                           uint8_t id_syn_ele);
static void decode_cpe(NeAACDecHandle hDecoder, NeAACDecFrameInfo *hInfo, bitfile *ld,
                       uint8_t id_syn_ele);
static uint8_t single_lfe_channel_element(NeAACDecHandle hDecoder, bitfile *ld,
                                          uint8_t channel, uint8_t *tag);
static uint8_t channel_pair_element(NeAACDecHandle hDecoder, bitfile *ld,
                                    uint8_t channel, uint8_t *tag);
#ifdef COUPLING_DEC
static uint8_t coupling_channel_element(NeAACDecHandle hDecoder, bitfile *ld);
#endif
static uint16_t data_stream_element(NeAACDecHandle hDecoder, bitfile *ld);
static uint8_t program_config_element(program_config *pce, bitfile *ld);
static uint8_t fill_element(NeAACDecHandle hDecoder, bitfile *ld, drc_info *drc
#ifdef SBR_DEC
                            ,uint8_t sbr_ele
#endif
                            );
static uint8_t individual_channel_stream(NeAACDecHandle hDecoder, element *ele,
                                         bitfile *ld, ic_stream *ics, uint8_t scal_flag,
                                         int16_t *spec_data);
static uint8_t ics_info(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld,
                        uint8_t common_window);
static uint8_t section_data(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld);
static uint8_t scale_factor_data(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld);
#ifdef SSR_DEC
static void gain_control_data(bitfile *ld, ic_stream *ics);
#endif
static uint8_t spectral_data(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld,
                             int16_t *spectral_data);
static uint16_t extension_payload(bitfile *ld, drc_info *drc, uint16_t count);
static uint8_t pulse_data(ic_stream *ics, pulse_info *pul, bitfile *ld);
static void tns_data(ic_stream *ics, tns_info *tns, bitfile *ld);
#ifdef LTP_DEC
static uint8_t ltp_data(NeAACDecHandle hDecoder, ic_stream *ics, ltp_info *ltp, bitfile *ld);
#endif
static uint8_t adts_fixed_header(adts_header *adts, bitfile *ld);
static void adts_variable_header(adts_header *adts, bitfile *ld);
static void adts_error_check(adts_header *adts, bitfile *ld);
static uint8_t dynamic_range_info(bitfile *ld, drc_info *drc);
static uint8_t excluded_channels(bitfile *ld, drc_info *drc);
#ifdef SCALABLE_DEC
static int8_t aac_scalable_main_header(NeAACDecHandle hDecoder, ic_stream *ics1, ic_stream *ics2,
                                       bitfile *ld, uint8_t this_layer_stereo);
#endif


/* Table 4.4.1 */
int8_t GASpecificConfig(bitfile *ld, mp4AudioSpecificConfig *mp4ASC,
                        program_config *pce_out)
{
    program_config pce;

    /* 1024 or 960 */
    mp4ASC->frameLengthFlag = faad_get1bit(ld
        DEBUGVAR(1,138,"GASpecificConfig(): FrameLengthFlag"));
#ifndef ALLOW_SMALL_FRAMELENGTH
    if (mp4ASC->frameLengthFlag == 1)
        return -3;
#endif

    mp4ASC->dependsOnCoreCoder = faad_get1bit(ld
        DEBUGVAR(1,139,"GASpecificConfig(): DependsOnCoreCoder"));
    if (mp4ASC->dependsOnCoreCoder == 1)
    {
        mp4ASC->coreCoderDelay = (uint16_t)faad_getbits(ld, 14
            DEBUGVAR(1,140,"GASpecificConfig(): CoreCoderDelay"));
    }

    mp4ASC->extensionFlag = faad_get1bit(ld DEBUGVAR(1,141,"GASpecificConfig(): ExtensionFlag"));
    if (mp4ASC->channelsConfiguration == 0)
    {
        if (program_config_element(&pce, ld))
            return -3;
        //mp4ASC->channelsConfiguration = pce.channels;

        if (pce_out != NULL)
            memcpy(pce_out, &pce, sizeof(program_config));

        /*
        if (pce.num_valid_cc_elements)
            return -3;
        */
    }

#ifdef ERROR_RESILIENCE
    if (mp4ASC->extensionFlag == 1)
    {
        /* Error resilience not supported yet */
        if (mp4ASC->objectTypeIndex >= ER_OBJECT_START)
        {
            mp4ASC->aacSectionDataResilienceFlag = faad_get1bit(ld
                DEBUGVAR(1,144,"GASpecificConfig(): aacSectionDataResilienceFlag"));
            mp4ASC->aacScalefactorDataResilienceFlag = faad_get1bit(ld
                DEBUGVAR(1,145,"GASpecificConfig(): aacScalefactorDataResilienceFlag"));
            mp4ASC->aacSpectralDataResilienceFlag = faad_get1bit(ld
                DEBUGVAR(1,146,"GASpecificConfig(): aacSpectralDataResilienceFlag"));

            /* 1 bit: extensionFlag3 */
        }
    }
#endif

    return 0;
}

/* Table 4.4.2 */
/* An MPEG-4 Audio decoder is only required to follow the Program
   Configuration Element in GASpecificConfig(). The decoder shall ignore
   any Program Configuration Elements that may occur in raw data blocks.
   PCEs transmitted in raw data blocks cannot be used to convey decoder
   configuration information.
*/
static uint8_t program_config_element(program_config *pce, bitfile *ld)
{
    uint8_t i;

    memset(pce, 0, sizeof(program_config));

    pce->channels = 0;

    pce->element_instance_tag = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,10,"program_config_element(): element_instance_tag"));

    pce->object_type = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,11,"program_config_element(): object_type"));
    pce->sf_index = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,12,"program_config_element(): sf_index"));
    pce->num_front_channel_elements = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,13,"program_config_element(): num_front_channel_elements"));
    pce->num_side_channel_elements = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,14,"program_config_element(): num_side_channel_elements"));
    pce->num_back_channel_elements = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,15,"program_config_element(): num_back_channel_elements"));
    pce->num_lfe_channel_elements = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,16,"program_config_element(): num_lfe_channel_elements"));
    pce->num_assoc_data_elements = (uint8_t)faad_getbits(ld, 3
        DEBUGVAR(1,17,"program_config_element(): num_assoc_data_elements"));
    pce->num_valid_cc_elements = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,18,"program_config_element(): num_valid_cc_elements"));

    pce->mono_mixdown_present = faad_get1bit(ld
        DEBUGVAR(1,19,"program_config_element(): mono_mixdown_present"));
    if (pce->mono_mixdown_present == 1)
    {
        pce->mono_mixdown_element_number = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,20,"program_config_element(): mono_mixdown_element_number"));
    }

    pce->stereo_mixdown_present = faad_get1bit(ld
        DEBUGVAR(1,21,"program_config_element(): stereo_mixdown_present"));
    if (pce->stereo_mixdown_present == 1)
    {
        pce->stereo_mixdown_element_number = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,22,"program_config_element(): stereo_mixdown_element_number"));
    }

    pce->matrix_mixdown_idx_present = faad_get1bit(ld
        DEBUGVAR(1,23,"program_config_element(): matrix_mixdown_idx_present"));
    if (pce->matrix_mixdown_idx_present == 1)
    {
        pce->matrix_mixdown_idx = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,24,"program_config_element(): matrix_mixdown_idx"));
        pce->pseudo_surround_enable = faad_get1bit(ld
            DEBUGVAR(1,25,"program_config_element(): pseudo_surround_enable"));
    }

    for (i = 0; i < pce->num_front_channel_elements; i++)
    {
        pce->front_element_is_cpe[i] = faad_get1bit(ld
            DEBUGVAR(1,26,"program_config_element(): front_element_is_cpe"));
        pce->front_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,27,"program_config_element(): front_element_tag_select"));

        if (pce->front_element_is_cpe[i] & 1)
        {
            pce->cpe_channel[pce->front_element_tag_select[i]] = pce->channels;
            pce->num_front_channels += 2;
            pce->channels += 2;
        } else {
            pce->sce_channel[pce->front_element_tag_select[i]] = pce->channels;
            pce->num_front_channels++;
            pce->channels++;
        }
    }

    for (i = 0; i < pce->num_side_channel_elements; i++)
    {
        pce->side_element_is_cpe[i] = faad_get1bit(ld
            DEBUGVAR(1,28,"program_config_element(): side_element_is_cpe"));
        pce->side_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,29,"program_config_element(): side_element_tag_select"));

        if (pce->side_element_is_cpe[i] & 1)
        {
            pce->cpe_channel[pce->side_element_tag_select[i]] = pce->channels;
            pce->num_side_channels += 2;
            pce->channels += 2;
        } else {
            pce->sce_channel[pce->side_element_tag_select[i]] = pce->channels;
            pce->num_side_channels++;
            pce->channels++;
        }
    }

    for (i = 0; i < pce->num_back_channel_elements; i++)
    {
        pce->back_element_is_cpe[i] = faad_get1bit(ld
            DEBUGVAR(1,30,"program_config_element(): back_element_is_cpe"));
        pce->back_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,31,"program_config_element(): back_element_tag_select"));

        if (pce->back_element_is_cpe[i] & 1)
        {
            pce->cpe_channel[pce->back_element_tag_select[i]] = pce->channels;
            pce->channels += 2;
            pce->num_back_channels += 2;
        } else {
            pce->sce_channel[pce->back_element_tag_select[i]] = pce->channels;
            pce->num_back_channels++;
            pce->channels++;
        }
    }

    for (i = 0; i < pce->num_lfe_channel_elements; i++)
    {
        pce->lfe_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,32,"program_config_element(): lfe_element_tag_select"));

        pce->sce_channel[pce->lfe_element_tag_select[i]] = pce->channels;
        pce->num_lfe_channels++;
        pce->channels++;
    }

    for (i = 0; i < pce->num_assoc_data_elements; i++)
        pce->assoc_data_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,33,"program_config_element(): assoc_data_element_tag_select"));

    for (i = 0; i < pce->num_valid_cc_elements; i++)
    {
        pce->cc_element_is_ind_sw[i] = faad_get1bit(ld
            DEBUGVAR(1,34,"program_config_element(): cc_element_is_ind_sw"));
        pce->valid_cc_element_tag_select[i] = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,35,"program_config_element(): valid_cc_element_tag_select"));
    }

    faad_byte_align(ld);

    pce->comment_field_bytes = (uint8_t)faad_getbits(ld, 8
        DEBUGVAR(1,36,"program_config_element(): comment_field_bytes"));

    for (i = 0; i < pce->comment_field_bytes; i++)
    {
        pce->comment_field_data[i] = (uint8_t)faad_getbits(ld, 8
            DEBUGVAR(1,37,"program_config_element(): comment_field_data"));
    }
    pce->comment_field_data[i] = 0;

    if (pce->channels > MAX_CHANNELS)
        return 22;

    return 0;
}

static void decode_sce_lfe(NeAACDecHandle hDecoder,
                           NeAACDecFrameInfo *hInfo, bitfile *ld,
                           uint8_t id_syn_ele)
{
    uint8_t channels = hDecoder->fr_channels;
    uint8_t tag = 0;

    if (channels+1 > MAX_CHANNELS)
    {
        hInfo->error = 12;
        return;
    }
    if (hDecoder->fr_ch_ele+1 > MAX_SYNTAX_ELEMENTS)
    {
        hInfo->error = 13;
        return;
    }

    /* for SCE hDecoder->element_output_channels[] is not set here because this
       can become 2 when some form of Parametric Stereo coding is used
    */

    /* save the syntax element id */
    hDecoder->element_id[hDecoder->fr_ch_ele] = id_syn_ele;

    /* decode the element */
    hInfo->error = single_lfe_channel_element(hDecoder, ld, channels, &tag);

    /* map output channels position to internal data channels */
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 2)
    {
        /* this might be faulty when pce_set is true */
        hDecoder->internal_channel[channels] = channels;
        hDecoder->internal_channel[channels+1] = channels+1;
    } else {
        if (hDecoder->pce_set)
            hDecoder->internal_channel[hDecoder->pce.sce_channel[tag]] = channels;
        else
            hDecoder->internal_channel[channels] = channels;
    }

    hDecoder->fr_channels += hDecoder->element_output_channels[hDecoder->fr_ch_ele];
    hDecoder->fr_ch_ele++;
}

static void decode_cpe(NeAACDecHandle hDecoder, NeAACDecFrameInfo *hInfo, bitfile *ld,
                       uint8_t id_syn_ele)
{
    uint8_t channels = hDecoder->fr_channels;
    uint8_t tag = 0;

    if (channels+2 > MAX_CHANNELS)
    {
        hInfo->error = 12;
        return;
    }
    if (hDecoder->fr_ch_ele+1 > MAX_SYNTAX_ELEMENTS)
    {
        hInfo->error = 13;
        return;
    }

    /* for CPE the number of output channels is always 2 */
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 0)
    {
        /* element_output_channels not set yet */
        hDecoder->element_output_channels[hDecoder->fr_ch_ele] = 2;
    } else if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] != 2) {
        /* element inconsistency */
        hInfo->error = 21;
        return;
    }

    /* save the syntax element id */
    hDecoder->element_id[hDecoder->fr_ch_ele] = id_syn_ele;

    /* decode the element */
    hInfo->error = channel_pair_element(hDecoder, ld, channels, &tag);

    /* map output channel position to internal data channels */
    if (hDecoder->pce_set)
    {
        hDecoder->internal_channel[hDecoder->pce.cpe_channel[tag]] = channels;
        hDecoder->internal_channel[hDecoder->pce.cpe_channel[tag]+1] = channels+1;
    } else {
        hDecoder->internal_channel[channels] = channels;
        hDecoder->internal_channel[channels+1] = channels+1;
    }

    hDecoder->fr_channels += 2;
    hDecoder->fr_ch_ele++;
}

void raw_data_block(NeAACDecHandle hDecoder, NeAACDecFrameInfo *hInfo,
                    bitfile *ld, program_config *pce, drc_info *drc)
{
    uint8_t id_syn_ele;

    hDecoder->fr_channels = 0;
    hDecoder->fr_ch_ele = 0;
    hDecoder->first_syn_ele = 25;
    hDecoder->has_lfe = 0;

#ifdef ERROR_RESILIENCE
    if (hDecoder->object_type < ER_OBJECT_START)
    {
#endif
        /* Table 4.4.3: raw_data_block() */
        while ((id_syn_ele = (uint8_t)faad_getbits(ld, LEN_SE_ID
            DEBUGVAR(1,4,"NeAACDecDecode(): id_syn_ele"))) != ID_END)
        {
            switch (id_syn_ele) {
            case ID_SCE:
                if (hDecoder->first_syn_ele == 25) hDecoder->first_syn_ele = id_syn_ele;
                decode_sce_lfe(hDecoder, hInfo, ld, id_syn_ele);
                if (hInfo->error > 0)
                    return;
                break;
            case ID_CPE:
                if (hDecoder->first_syn_ele == 25) hDecoder->first_syn_ele = id_syn_ele;
                decode_cpe(hDecoder, hInfo, ld, id_syn_ele);
                if (hInfo->error > 0)
                    return;
                break;
            case ID_LFE:
                hDecoder->has_lfe++;
                decode_sce_lfe(hDecoder, hInfo, ld, id_syn_ele);
                if (hInfo->error > 0)
                    return;
                break;
            case ID_CCE: /* not implemented yet, but skip the bits */
#ifdef COUPLING_DEC
                hInfo->error = coupling_channel_element(hDecoder, ld);
#else
                hInfo->error = 6;
#endif
                if (hInfo->error > 0)
                    return;
                break;
            case ID_DSE:
                data_stream_element(hDecoder, ld);
                break;
            case ID_PCE:
                /* 14496-4: 5.6.4.1.2.1.3: */
                /* program_configuration_element()'s in access units shall be ignored */
                program_config_element(pce, ld);
                //if ((hInfo->error = program_config_element(pce, ld)) > 0)
                //    return;
                //hDecoder->pce_set = 1;
                break;
            case ID_FIL:
                /* one sbr_info describes a channel_element not a channel! */
                /* if we encounter SBR data here: error */
                /* SBR data will be read directly in the SCE/LFE/CPE element */
                if ((hInfo->error = fill_element(hDecoder, ld, drc
#ifdef SBR_DEC
                    , INVALID_SBR_ELEMENT
#endif
                    )) > 0)
                    return;
                break;
            }
        }
#ifdef ERROR_RESILIENCE
    } else {
        /* Table 262: er_raw_data_block() */
        switch (hDecoder->channelConfiguration)
        {
        case 1:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if (hInfo->error > 0)
                return;
            break;
        case 2:
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if (hInfo->error > 0)
                return;
            break;
        case 3:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if (hInfo->error > 0)
                return;
            break;
        case 4:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            if (hInfo->error > 0)
                return;
            break;
        case 5:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            if (hInfo->error > 0)
                return;
            break;
        case 6:
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            decode_sce_lfe(hDecoder, hInfo, ld, ID_LFE);
            if (hInfo->error > 0)
                return;
            break;
        case 7: /* 8 channels */
            decode_sce_lfe(hDecoder, hInfo, ld, ID_SCE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            decode_cpe(hDecoder, hInfo, ld, ID_CPE);
            decode_sce_lfe(hDecoder, hInfo, ld, ID_LFE);
            if (hInfo->error > 0)
                return;
            break;
        default:
            hInfo->error = 7;
            return;
        }
#if 0
        cnt = bits_to_decode() / 8;
        while (cnt >= 1)
        {
            cnt -= extension_payload(cnt);
        }
#endif
    }
#endif

    /* new in corrigendum 14496-3:2002 */
#ifdef DRM
    if (hDecoder->object_type != DRM_ER_LC)
#endif
    {
        faad_byte_align(ld);
    }

    return;
}

/* Table 4.4.4 and */
/* Table 4.4.9 */
static uint8_t single_lfe_channel_element(NeAACDecHandle hDecoder, bitfile *ld,
                                          uint8_t channel, uint8_t *tag)
{
    uint8_t retval = 0;
    element sce = {0};
    ic_stream *ics = &(sce.ics1);
    ALIGN int16_t spec_data[1024] = {0};

    sce.element_instance_tag = (uint8_t)faad_getbits(ld, LEN_TAG
        DEBUGVAR(1,38,"single_lfe_channel_element(): element_instance_tag"));

    *tag = sce.element_instance_tag;
    sce.channel = channel;
    sce.paired_channel = -1;

    retval = individual_channel_stream(hDecoder, &sce, ld, ics, 0, spec_data);
    if (retval > 0)
        return retval;

#ifdef SBR_DEC
    /* check if next bitstream element is a fill element */
    /* if so, read it now so SBR decoding can be done in case of a file with SBR */
    if (faad_showbits(ld, LEN_SE_ID) == ID_FIL)
    {
        faad_flushbits(ld, LEN_SE_ID);

        /* one sbr_info describes a channel_element not a channel! */
        if ((retval = fill_element(hDecoder, ld, hDecoder->drc, hDecoder->fr_ch_ele)) > 0)
        {
            return retval;
        }
    }
#endif

    /* noiseless coding is done, spectral reconstruction is done now */
    retval = reconstruct_single_channel(hDecoder, ics, &sce, spec_data);
    if (retval > 0)
        return retval;

    return 0;
}

/* Table 4.4.5 */
static uint8_t channel_pair_element(NeAACDecHandle hDecoder, bitfile *ld,
                                    uint8_t channels, uint8_t *tag)
{
    ALIGN int16_t spec_data1[1024] = {0};
    ALIGN int16_t spec_data2[1024] = {0};
    element cpe = {0};
    ic_stream *ics1 = &(cpe.ics1);
    ic_stream *ics2 = &(cpe.ics2);
    uint8_t result;

    cpe.channel        = channels;
    cpe.paired_channel = channels+1;

    cpe.element_instance_tag = (uint8_t)faad_getbits(ld, LEN_TAG
        DEBUGVAR(1,39,"channel_pair_element(): element_instance_tag"));
    *tag = cpe.element_instance_tag;

    if ((cpe.common_window = faad_get1bit(ld
        DEBUGVAR(1,40,"channel_pair_element(): common_window"))) & 1)
    {
        /* both channels have common ics information */
        if ((result = ics_info(hDecoder, ics1, ld, cpe.common_window)) > 0)
            return result;

        ics1->ms_mask_present = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,41,"channel_pair_element(): ms_mask_present"));
        if (ics1->ms_mask_present == 1)
        {
            uint8_t g, sfb;
            for (g = 0; g < ics1->num_window_groups; g++)
            {
                for (sfb = 0; sfb < ics1->max_sfb; sfb++)
                {
                    ics1->ms_used[g][sfb] = faad_get1bit(ld
                        DEBUGVAR(1,42,"channel_pair_element(): faad_get1bit"));
                }
            }
        }

#ifdef ERROR_RESILIENCE
        if ((hDecoder->object_type >= ER_OBJECT_START) && (ics1->predictor_data_present))
        {
            if ((
#ifdef LTP_DEC
                ics1->ltp.data_present =
#endif
                faad_get1bit(ld DEBUGVAR(1,50,"channel_pair_element(): ltp.data_present"))) & 1)
            {
#ifdef LTP_DEC
                if ((result = ltp_data(hDecoder, ics1, &(ics1->ltp), ld)) > 0)
                {
                    return result;
                }
#else
                return 26;
#endif
            }
        }
#endif

        memcpy(ics2, ics1, sizeof(ic_stream));
    } else {
        ics1->ms_mask_present = 0;
    }

    if ((result = individual_channel_stream(hDecoder, &cpe, ld, ics1,
        0, spec_data1)) > 0)
    {
        return result;
    }

#ifdef ERROR_RESILIENCE
    if (cpe.common_window && (hDecoder->object_type >= ER_OBJECT_START) &&
        (ics1->predictor_data_present))
    {
        if ((
#ifdef LTP_DEC
            ics1->ltp2.data_present =
#endif
            faad_get1bit(ld DEBUGVAR(1,50,"channel_pair_element(): ltp.data_present"))) & 1)
        {
#ifdef LTP_DEC
            if ((result = ltp_data(hDecoder, ics1, &(ics1->ltp2), ld)) > 0)
            {
                return result;
            }
#else
            return 26;
#endif
        }
    }
#endif

    if ((result = individual_channel_stream(hDecoder, &cpe, ld, ics2,
        0, spec_data2)) > 0)
    {
        return result;
    }

#ifdef SBR_DEC
    /* check if next bitstream element is a fill element */
    /* if so, read it now so SBR decoding can be done in case of a file with SBR */
    if (faad_showbits(ld, LEN_SE_ID) == ID_FIL)
    {
        faad_flushbits(ld, LEN_SE_ID);

        /* one sbr_info describes a channel_element not a channel! */
        if ((result = fill_element(hDecoder, ld, hDecoder->drc, hDecoder->fr_ch_ele)) > 0)
        {
            return result;
        }
    }
#endif

    /* noiseless coding is done, spectral reconstruction is done now */
    if ((result = reconstruct_channel_pair(hDecoder, ics1, ics2, &cpe,
        spec_data1, spec_data2)) > 0)
    {
        return result;
    }

    return 0;
}

/* Table 4.4.6 */
static uint8_t ics_info(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld,
                        uint8_t common_window)
{
    uint8_t retval = 0;

    /* ics->ics_reserved_bit = */ faad_get1bit(ld
        DEBUGVAR(1,43,"ics_info(): ics_reserved_bit"));
    ics->window_sequence = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,44,"ics_info(): window_sequence"));
    ics->window_shape = faad_get1bit(ld
        DEBUGVAR(1,45,"ics_info(): window_shape"));

    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
    {
        ics->max_sfb = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,46,"ics_info(): max_sfb (short)"));
        ics->scale_factor_grouping = (uint8_t)faad_getbits(ld, 7
            DEBUGVAR(1,47,"ics_info(): scale_factor_grouping"));
    } else {
        ics->max_sfb = (uint8_t)faad_getbits(ld, 6
            DEBUGVAR(1,48,"ics_info(): max_sfb (long)"));
    }

    /* get the grouping information */
    if ((retval = window_grouping_info(hDecoder, ics)) > 0)
        return retval;

    /* should be an error */
    /* check the range of max_sfb */
    if (ics->max_sfb > ics->num_swb)
        return 16;

    if (ics->window_sequence != EIGHT_SHORT_SEQUENCE)
    {
        if ((ics->predictor_data_present = faad_get1bit(ld
            DEBUGVAR(1,49,"ics_info(): predictor_data_present"))) & 1)
        {
            if (hDecoder->object_type == MAIN) /* MPEG2 style AAC predictor */
            {
                uint8_t sfb;

                uint8_t limit = min(ics->max_sfb, max_pred_sfb(hDecoder->sf_index));
#ifdef MAIN_DEC
                ics->pred.limit = limit;
#endif

                if ((
#ifdef MAIN_DEC
                    ics->pred.predictor_reset =
#endif
                    faad_get1bit(ld DEBUGVAR(1,53,"ics_info(): pred.predictor_reset"))) & 1)
                {
#ifdef MAIN_DEC
                    ics->pred.predictor_reset_group_number =
#endif
                        (uint8_t)faad_getbits(ld, 5 DEBUGVAR(1,54,"ics_info(): pred.predictor_reset_group_number"));
                }

                for (sfb = 0; sfb < limit; sfb++)
                {
#ifdef MAIN_DEC
                    ics->pred.prediction_used[sfb] =
#endif
                        faad_get1bit(ld DEBUGVAR(1,55,"ics_info(): pred.prediction_used"));
                }
            }
#ifdef LTP_DEC
            else { /* Long Term Prediction */
                if (hDecoder->object_type < ER_OBJECT_START)
                {
                    if ((ics->ltp.data_present = faad_get1bit(ld
                        DEBUGVAR(1,50,"ics_info(): ltp.data_present"))) & 1)
                    {
                        if ((retval = ltp_data(hDecoder, ics, &(ics->ltp), ld)) > 0)
                        {
                            return retval;
                        }
                    }
                    if (common_window)
                    {
                        if ((ics->ltp2.data_present = faad_get1bit(ld
                            DEBUGVAR(1,51,"ics_info(): ltp2.data_present"))) & 1)
                        {
                            if ((retval = ltp_data(hDecoder, ics, &(ics->ltp2), ld)) > 0)
                            {
                                return retval;
                            }
                        }
                    }
                }
#ifdef ERROR_RESILIENCE
                if (!common_window && (hDecoder->object_type >= ER_OBJECT_START))
                {
                    if ((ics->ltp.data_present = faad_get1bit(ld
                        DEBUGVAR(1,50,"ics_info(): ltp.data_present"))) & 1)
                    {
                        ltp_data(hDecoder, ics, &(ics->ltp), ld);
                    }
                }
#endif
            }
#endif
        }
    }

    return retval;
}

/* Table 4.4.7 */
static uint8_t pulse_data(ic_stream *ics, pulse_info *pul, bitfile *ld)
{
    uint8_t i;

    pul->number_pulse = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,56,"pulse_data(): number_pulse"));
    pul->pulse_start_sfb = (uint8_t)faad_getbits(ld, 6
        DEBUGVAR(1,57,"pulse_data(): pulse_start_sfb"));

    /* check the range of pulse_start_sfb */
    if (pul->pulse_start_sfb > ics->num_swb)
        return 16;

    for (i = 0; i < pul->number_pulse+1; i++)
    {
        pul->pulse_offset[i] = (uint8_t)faad_getbits(ld, 5
            DEBUGVAR(1,58,"pulse_data(): pulse_offset"));
#if 0
        printf("%d\n", pul->pulse_offset[i]);
#endif
        pul->pulse_amp[i] = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,59,"pulse_data(): pulse_amp"));
#if 0
        printf("%d\n", pul->pulse_amp[i]);
#endif
    }

    return 0;
}

#ifdef COUPLING_DEC
/* Table 4.4.8: Currently just for skipping the bits... */
static uint8_t coupling_channel_element(NeAACDecHandle hDecoder, bitfile *ld)
{
    uint8_t c, result = 0;
    uint8_t ind_sw_cce_flag = 0;
    uint8_t num_gain_element_lists = 0;
    uint8_t num_coupled_elements = 0;

    element el_empty = {0};
    ic_stream ics_empty = {0};
    int16_t sh_data[1024];

    c = faad_getbits(ld, LEN_TAG
        DEBUGVAR(1,900,"coupling_channel_element(): element_instance_tag"));

    ind_sw_cce_flag = faad_get1bit(ld
        DEBUGVAR(1,901,"coupling_channel_element(): ind_sw_cce_flag"));
    num_coupled_elements = faad_getbits(ld, 3
        DEBUGVAR(1,902,"coupling_channel_element(): num_coupled_elements"));

    for (c = 0; c < num_coupled_elements + 1; c++)
    {
        uint8_t cc_target_is_cpe, cc_target_tag_select;

        num_gain_element_lists++;

        cc_target_is_cpe = faad_get1bit(ld
            DEBUGVAR(1,903,"coupling_channel_element(): cc_target_is_cpe"));
        cc_target_tag_select = faad_getbits(ld, 4
            DEBUGVAR(1,904,"coupling_channel_element(): cc_target_tag_select"));

        if (cc_target_is_cpe)
        {
            uint8_t cc_l = faad_get1bit(ld
                DEBUGVAR(1,905,"coupling_channel_element(): cc_l"));
            uint8_t cc_r = faad_get1bit(ld
                DEBUGVAR(1,906,"coupling_channel_element(): cc_r"));

            if (cc_l && cc_r)
                num_gain_element_lists++;
        }
    }

    faad_get1bit(ld
        DEBUGVAR(1,907,"coupling_channel_element(): cc_domain"));
    faad_get1bit(ld
        DEBUGVAR(1,908,"coupling_channel_element(): gain_element_sign"));
    faad_getbits(ld, 2
        DEBUGVAR(1,909,"coupling_channel_element(): gain_element_scale"));

    if ((result = individual_channel_stream(hDecoder, &el_empty, ld, &ics_empty,
        0, sh_data)) > 0)
    {
        return result;
    }

    for (c = 1; c < num_gain_element_lists; c++)
    {
        uint8_t cge;

        if (ind_sw_cce_flag)
        {
            cge = 1;
        } else {
            cge = faad_get1bit(ld
                DEBUGVAR(1,910,"coupling_channel_element(): common_gain_element_present"));
        }

        if (cge)
        {
            huffman_scale_factor(ld);
        } else {
            uint8_t g, sfb;

            for (g = 0; g < ics_empty.num_window_groups; g++)
            {
                for (sfb = 0; sfb < ics_empty.max_sfb; sfb++)
                {
                    if (ics_empty.sfb_cb[g][sfb] != ZERO_HCB)
                        huffman_scale_factor(ld);
                }
            }
        }
    }

    return 0;
}
#endif

/* Table 4.4.10 */
static uint16_t data_stream_element(NeAACDecHandle hDecoder, bitfile *ld)
{
    uint8_t byte_aligned;
    uint16_t i, count;

    /* element_instance_tag = */ faad_getbits(ld, LEN_TAG
        DEBUGVAR(1,60,"data_stream_element(): element_instance_tag"));
    byte_aligned = faad_get1bit(ld
        DEBUGVAR(1,61,"data_stream_element(): byte_aligned"));
    count = (uint16_t)faad_getbits(ld, 8
        DEBUGVAR(1,62,"data_stream_element(): count"));
    if (count == 255)
    {
        count += (uint16_t)faad_getbits(ld, 8
            DEBUGVAR(1,63,"data_stream_element(): extra count"));
    }
    if (byte_aligned)
        faad_byte_align(ld);

    for (i = 0; i < count; i++)
    {
        faad_getbits(ld, LEN_BYTE
            DEBUGVAR(1,64,"data_stream_element(): data_stream_byte"));
    }

    return count;
}

/* Table 4.4.11 */
static uint8_t fill_element(NeAACDecHandle hDecoder, bitfile *ld, drc_info *drc
#ifdef SBR_DEC
                            ,uint8_t sbr_ele
#endif
                            )
{
    uint16_t count;
#ifdef SBR_DEC
    uint8_t bs_extension_type;
#endif

    count = (uint16_t)faad_getbits(ld, 4
        DEBUGVAR(1,65,"fill_element(): count"));
    if (count == 15)
    {
        count += (uint16_t)faad_getbits(ld, 8
            DEBUGVAR(1,66,"fill_element(): extra count")) - 1;
    }

    if (count > 0)
    {
#ifdef SBR_DEC
        bs_extension_type = (uint8_t)faad_showbits(ld, 4);

        if ((bs_extension_type == EXT_SBR_DATA) ||
            (bs_extension_type == EXT_SBR_DATA_CRC))
        {
            if (sbr_ele == INVALID_SBR_ELEMENT)
                return 24;

            if (!hDecoder->sbr[sbr_ele])
            {
                hDecoder->sbr[sbr_ele] = sbrDecodeInit(hDecoder->frameLength,
                    hDecoder->element_id[sbr_ele], 2*get_sample_rate(hDecoder->sf_index),
                    hDecoder->downSampledSBR
#ifdef DRM
                    , 0
#endif
                    );
            }

            hDecoder->sbr_present_flag = 1;

            /* parse the SBR data */
            hDecoder->sbr[sbr_ele]->ret = sbr_extension_data(ld, hDecoder->sbr[sbr_ele], count);

#if 0
            if (hDecoder->sbr[sbr_ele]->ret > 0)
            {
                printf("%s\n", NeAACDecGetErrorMessage(hDecoder->sbr[sbr_ele]->ret));
            }
#endif

#if (defined(PS_DEC) || defined(DRM_PS))
            if (hDecoder->sbr[sbr_ele]->ps_used)
            {
                hDecoder->ps_used[sbr_ele] = 1;

                /* set element independent flag to 1 as well */
                hDecoder->ps_used_global = 1;
            }
#endif
        } else {
#endif
            while (count > 0)
            {
                count -= extension_payload(ld, drc, count);
            }
#ifdef SBR_DEC
        }
#endif
    }

    return 0;
}

/* Table 4.4.12 */
#ifdef SSR_DEC
static void gain_control_data(bitfile *ld, ic_stream *ics)
{
    uint8_t bd, wd, ad;
    ssr_info *ssr = &(ics->ssr);

    ssr->max_band = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,1000,"gain_control_data(): max_band"));

    if (ics->window_sequence == ONLY_LONG_SEQUENCE)
    {
        for (bd = 1; bd <= ssr->max_band; bd++)
        {
            for (wd = 0; wd < 1; wd++)
            {
                ssr->adjust_num[bd][wd] = (uint8_t)faad_getbits(ld, 3
                    DEBUGVAR(1,1001,"gain_control_data(): adjust_num"));

                for (ad = 0; ad < ssr->adjust_num[bd][wd]; ad++)
                {
                    ssr->alevcode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4
                        DEBUGVAR(1,1002,"gain_control_data(): alevcode"));
                    ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 5
                        DEBUGVAR(1,1003,"gain_control_data(): aloccode"));
                }
            }
        }
    } else if (ics->window_sequence == LONG_START_SEQUENCE) {
        for (bd = 1; bd <= ssr->max_band; bd++)
        {
            for (wd = 0; wd < 2; wd++)
            {
                ssr->adjust_num[bd][wd] = (uint8_t)faad_getbits(ld, 3
                    DEBUGVAR(1,1001,"gain_control_data(): adjust_num"));

                for (ad = 0; ad < ssr->adjust_num[bd][wd]; ad++)
                {
                    ssr->alevcode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4
                        DEBUGVAR(1,1002,"gain_control_data(): alevcode"));
                    if (wd == 0)
                    {
                        ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4
                            DEBUGVAR(1,1003,"gain_control_data(): aloccode"));
                    } else {
                        ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 2
                            DEBUGVAR(1,1003,"gain_control_data(): aloccode"));
                    }
                }
            }
        }
    } else if (ics->window_sequence == EIGHT_SHORT_SEQUENCE) {
        for (bd = 1; bd <= ssr->max_band; bd++)
        {
            for (wd = 0; wd < 8; wd++)
            {
                ssr->adjust_num[bd][wd] = (uint8_t)faad_getbits(ld, 3
                    DEBUGVAR(1,1001,"gain_control_data(): adjust_num"));

                for (ad = 0; ad < ssr->adjust_num[bd][wd]; ad++)
                {
                    ssr->alevcode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4
                        DEBUGVAR(1,1002,"gain_control_data(): alevcode"));
                    ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 2
                        DEBUGVAR(1,1003,"gain_control_data(): aloccode"));
                }
            }
        }
    } else if (ics->window_sequence == LONG_STOP_SEQUENCE) {
        for (bd = 1; bd <= ssr->max_band; bd++)
        {
            for (wd = 0; wd < 2; wd++)
            {
                ssr->adjust_num[bd][wd] = (uint8_t)faad_getbits(ld, 3
                    DEBUGVAR(1,1001,"gain_control_data(): adjust_num"));

                for (ad = 0; ad < ssr->adjust_num[bd][wd]; ad++)
                {
                    ssr->alevcode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4
                        DEBUGVAR(1,1002,"gain_control_data(): alevcode"));

                    if (wd == 0)
                    {
                        ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 4
                            DEBUGVAR(1,1003,"gain_control_data(): aloccode"));
                    } else {
                        ssr->aloccode[bd][wd][ad] = (uint8_t)faad_getbits(ld, 5
                            DEBUGVAR(1,1003,"gain_control_data(): aloccode"));
                    }
                }
            }
        }
    }
}
#endif

#ifdef SCALABLE_DEC
/* Table 4.4.13 ASME */
void aac_scalable_main_element(NeAACDecHandle hDecoder, NeAACDecFrameInfo *hInfo,
                               bitfile *ld, program_config *pce, drc_info *drc)
{
    uint8_t retval = 0;
    uint8_t channels = hDecoder->fr_channels = 0;
    uint8_t ch;
    uint8_t this_layer_stereo = (hDecoder->channelConfiguration > 1) ? 1 : 0;
    element cpe = {0};
    ic_stream *ics1 = &(cpe.ics1);
    ic_stream *ics2 = &(cpe.ics2);
    int16_t *spec_data;
    ALIGN int16_t spec_data1[1024] = {0};
    ALIGN int16_t spec_data2[1024] = {0};

    hDecoder->fr_ch_ele = 0;

    hInfo->error = aac_scalable_main_header(hDecoder, ics1, ics2, ld, this_layer_stereo);
    if (hInfo->error > 0)
        return;

    cpe.common_window = 1;
    if (this_layer_stereo)
    {
        hDecoder->element_id[0] = ID_CPE;
        if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 0)
            hDecoder->element_output_channels[hDecoder->fr_ch_ele] = 2;
    } else {
        hDecoder->element_id[0] = ID_SCE;
    }

    for (ch = 0; ch < (this_layer_stereo ? 2 : 1); ch++)
    {
        ic_stream *ics;
        if (ch == 0)
        {
            ics = ics1;
            spec_data = spec_data1;
        } else {
            ics = ics2;
            spec_data = spec_data2;
        }

        hInfo->error = individual_channel_stream(hDecoder, &cpe, ld, ics, 1, spec_data);
        if (hInfo->error > 0)
            return;
    }

#ifdef DRM
#ifdef SBR_DEC
    /* In case of DRM we need to read the SBR info before channel reconstruction */
    if ((hDecoder->sbr_present_flag == 1) && (hDecoder->object_type == DRM_ER_LC))
    {
        bitfile ld_sbr = {0};
        uint32_t i;
        uint16_t count = 0;
        uint8_t *revbuffer;
        uint8_t *prevbufstart;
        uint8_t *pbufend;

        /* all forward bitreading should be finished at this point */
        uint32_t bitsconsumed = faad_get_processed_bits(ld);
        uint32_t buffer_size = faad_origbitbuffer_size(ld);
        uint8_t *buffer = (uint8_t*)faad_origbitbuffer(ld);

        if (bitsconsumed + 8 > buffer_size*8)
        {
            hInfo->error = 14;
            return;
        }

        if (!hDecoder->sbr[0])
        {
            hDecoder->sbr[0] = sbrDecodeInit(hDecoder->frameLength, hDecoder->element_id[0],
                2*get_sample_rate(hDecoder->sf_index), 0 /* ds SBR */, 1);
        }

        /* Reverse bit reading of SBR data in DRM audio frame */
        revbuffer = (uint8_t*)faad_malloc(buffer_size*sizeof(uint8_t));
        prevbufstart = revbuffer;
        pbufend = &buffer[buffer_size - 1];
        for (i = 0; i < buffer_size; i++)
            *prevbufstart++ = tabFlipbits[*pbufend--];

        /* Set SBR data */
        /* consider 8 bits from AAC-CRC */
        count = (uint16_t)bit2byte(buffer_size*8 - bitsconsumed);
        faad_initbits(&ld_sbr, revbuffer, count);

        hDecoder->sbr[0]->sample_rate = get_sample_rate(hDecoder->sf_index);
        hDecoder->sbr[0]->sample_rate *= 2;

        faad_getbits(&ld_sbr, 8); /* Skip 8-bit CRC */

        hDecoder->sbr[0]->ret = sbr_extension_data(&ld_sbr, hDecoder->sbr[0], count);
#if (defined(PS_DEC) || defined(DRM_PS))
        if (hDecoder->sbr[0]->ps_used)
        {
            hDecoder->ps_used[0] = 1;
            hDecoder->ps_used_global = 1;
        }
#endif

        /* check CRC */
        /* no need to check it if there was already an error */
        if (hDecoder->sbr[0]->ret == 0)
            hDecoder->sbr[0]->ret = (uint8_t)faad_check_CRC(&ld_sbr, (uint16_t)faad_get_processed_bits(&ld_sbr) - 8);

        /* SBR data was corrupted, disable it until the next header */
        if (hDecoder->sbr[0]->ret != 0)
        {
            hDecoder->sbr[0]->header_count = 0;  
        }

        faad_endbits(&ld_sbr);

        if (revbuffer)
            faad_free(revbuffer);
    }
#endif
#endif

    if (this_layer_stereo)
    {
        hInfo->error = reconstruct_channel_pair(hDecoder, ics1, ics2, &cpe, spec_data1, spec_data2);
        if (hInfo->error > 0)
            return;
    } else {
        hInfo->error = reconstruct_single_channel(hDecoder, ics1, &cpe, spec_data1);
        if (hInfo->error > 0)
            return;
    }

    /* map output channels position to internal data channels */
    if (hDecoder->element_output_channels[hDecoder->fr_ch_ele] == 2)
    {
        /* this might be faulty when pce_set is true */
        hDecoder->internal_channel[channels] = channels;
        hDecoder->internal_channel[channels+1] = channels+1;
    } else {
        hDecoder->internal_channel[channels] = channels;
    }

    hDecoder->fr_channels += hDecoder->element_output_channels[hDecoder->fr_ch_ele];
    hDecoder->fr_ch_ele++;

    return;
}

/* Table 4.4.15 */
static int8_t aac_scalable_main_header(NeAACDecHandle hDecoder, ic_stream *ics1, ic_stream *ics2,
                                       bitfile *ld, uint8_t this_layer_stereo)
{
    uint8_t retval = 0;
    uint8_t ch;
    ic_stream *ics;

    /* ics1->ics_reserved_bit = */ faad_get1bit(ld
        DEBUGVAR(1,300,"aac_scalable_main_header(): ics_reserved_bits"));
    ics1->window_sequence = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,301,"aac_scalable_main_header(): window_sequence"));
    ics1->window_shape = faad_get1bit(ld
        DEBUGVAR(1,302,"aac_scalable_main_header(): window_shape"));

    if (ics1->window_sequence == EIGHT_SHORT_SEQUENCE)
    {
        ics1->max_sfb = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,303,"aac_scalable_main_header(): max_sfb (short)"));
        ics1->scale_factor_grouping = (uint8_t)faad_getbits(ld, 7
            DEBUGVAR(1,304,"aac_scalable_main_header(): scale_factor_grouping"));
    } else {
        ics1->max_sfb = (uint8_t)faad_getbits(ld, 6
            DEBUGVAR(1,305,"aac_scalable_main_header(): max_sfb (long)"));
    }

    /* get the grouping information */
    if ((retval = window_grouping_info(hDecoder, ics1)) > 0)
        return retval;

    /* should be an error */
    /* check the range of max_sfb */
    if (ics1->max_sfb > ics1->num_swb)
        return 16;

    if (this_layer_stereo)
    {
        ics1->ms_mask_present = (uint8_t)faad_getbits(ld, 2
            DEBUGVAR(1,306,"aac_scalable_main_header(): ms_mask_present"));
        if (ics1->ms_mask_present == 1)
        {
            uint8_t g, sfb;
            for (g = 0; g < ics1->num_window_groups; g++)
            {
                for (sfb = 0; sfb < ics1->max_sfb; sfb++)
                {
                    ics1->ms_used[g][sfb] = faad_get1bit(ld
                        DEBUGVAR(1,307,"aac_scalable_main_header(): faad_get1bit"));
                }
            }
        }

        memcpy(ics2, ics1, sizeof(ic_stream));
    } else {
        ics1->ms_mask_present = 0;
    }

    if (0)
    {
        faad_get1bit(ld
            DEBUGVAR(1,308,"aac_scalable_main_header(): tns_channel_mono_layer"));
    }

    for (ch = 0; ch < (this_layer_stereo ? 2 : 1); ch++)
    {
        if (ch == 0)
            ics = ics1;
        else
            ics = ics2;

        if ( 1 /*!tvq_layer_pesent || (tns_aac_tvq_en[ch] == 1)*/)
        {
            if ((ics->tns_data_present = faad_get1bit(ld
                DEBUGVAR(1,309,"aac_scalable_main_header(): tns_data_present"))) & 1)
            {
#ifdef DRM
                /* different order of data units in DRM */
                if (hDecoder->object_type != DRM_ER_LC)
#endif
                {
                    tns_data(ics, &(ics->tns), ld);
                }
            }
        }
#if 0
        if (0 /*core_flag || tvq_layer_pesent*/)
        {
            if ((ch==0) || ((ch==1) && (core_stereo || tvq_stereo))
                diff_control_data();
            if (mono_stereo_flag)
                diff_control_data_lr();
        } else {
#endif
            if ((
#ifdef LTP_DEC
                ics->ltp.data_present =
#endif
                faad_get1bit(ld DEBUGVAR(1,310,"aac_scalable_main_header(): ltp.data_present"))) & 1)
            {
#ifdef LTP_DEC
                if ((retval = ltp_data(hDecoder, ics, &(ics->ltp), ld)) > 0)
                {
                    return retval;
                }
#else
                return 26;
#endif
            }
#if 0
        }
#endif
    }

    return 0;
}
#endif

/* Table 4.4.24 */
static uint8_t individual_channel_stream(NeAACDecHandle hDecoder, element *ele,
                                         bitfile *ld, ic_stream *ics, uint8_t scal_flag,
                                         int16_t *spec_data)
{
    uint8_t result;

    ics->global_gain = (uint8_t)faad_getbits(ld, 8
        DEBUGVAR(1,67,"individual_channel_stream(): global_gain"));

    if (!ele->common_window && !scal_flag)
    {
        if ((result = ics_info(hDecoder, ics, ld, ele->common_window)) > 0)
            return result;
    }

    if ((result = section_data(hDecoder, ics, ld)) > 0)
        return result;

    if ((result = scale_factor_data(hDecoder, ics, ld)) > 0)
        return result;

    if (!scal_flag)
    {
        /**
         **  NOTE: It could be that pulse data is available in scalable AAC too,
         **        as said in Amendment 1, this could be only the case for ER AAC,
         **        though. (have to check this out later)
         **/
        /* get pulse data */
        if ((ics->pulse_data_present = faad_get1bit(ld
            DEBUGVAR(1,68,"individual_channel_stream(): pulse_data_present"))) & 1)
        {
            if ((result = pulse_data(ics, &(ics->pul), ld)) > 0)
                return result;
        }

        /* get tns data */
        if ((ics->tns_data_present = faad_get1bit(ld
            DEBUGVAR(1,69,"individual_channel_stream(): tns_data_present"))) & 1)
        {
#ifdef ERROR_RESILIENCE
            if (hDecoder->object_type < ER_OBJECT_START)
#endif
                tns_data(ics, &(ics->tns), ld);
        }

        /* get gain control data */
        if ((ics->gain_control_data_present = faad_get1bit(ld
            DEBUGVAR(1,70,"individual_channel_stream(): gain_control_data_present"))) & 1)
        {
#ifdef SSR_DEC
            if (hDecoder->object_type != SSR)
                return 1;
            else
                gain_control_data(ld, ics);
#else
            return 1;
#endif
        }
    }

#ifdef ERROR_RESILIENCE
    if (hDecoder->aacSpectralDataResilienceFlag)
    {
        ics->length_of_reordered_spectral_data = (uint16_t)faad_getbits(ld, 14
            DEBUGVAR(1,147,"individual_channel_stream(): length_of_reordered_spectral_data"));

        if (hDecoder->channelConfiguration == 2)
        {
            if (ics->length_of_reordered_spectral_data > 6144)
                ics->length_of_reordered_spectral_data = 6144;
        } else {
            if (ics->length_of_reordered_spectral_data > 12288)
                ics->length_of_reordered_spectral_data = 12288;
        }

        ics->length_of_longest_codeword = (uint8_t)faad_getbits(ld, 6
            DEBUGVAR(1,148,"individual_channel_stream(): length_of_longest_codeword"));
        if (ics->length_of_longest_codeword >= 49)
            ics->length_of_longest_codeword = 49;
    }

    /* RVLC spectral data is put here */
    if (hDecoder->aacScalefactorDataResilienceFlag)
    {
        if ((result = rvlc_decode_scale_factors(ics, ld)) > 0)
            return result;
    }

    if (hDecoder->object_type >= ER_OBJECT_START) 
    {
        if (ics->tns_data_present)
            tns_data(ics, &(ics->tns), ld);
    }

#ifdef DRM
    /* CRC check */
    if (hDecoder->object_type == DRM_ER_LC)
        if ((result = (uint8_t)faad_check_CRC(ld, (uint16_t)faad_get_processed_bits(ld) - 8)) > 0)
            return result;
#endif

    if (hDecoder->aacSpectralDataResilienceFlag)
    {
        /* error resilient spectral data decoding */
        if ((result = reordered_spectral_data(hDecoder, ics, ld, spec_data)) > 0)
        {
            return result;
        }
    } else {
#endif
        /* decode the spectral data */
        if ((result = spectral_data(hDecoder, ics, ld, spec_data)) > 0)
        {
            return result;
        }
#ifdef ERROR_RESILIENCE
    }
#endif

    /* pulse coding reconstruction */
    if (ics->pulse_data_present)
    {
        if (ics->window_sequence != EIGHT_SHORT_SEQUENCE)
        {
            if ((result = pulse_decode(ics, spec_data, hDecoder->frameLength)) > 0)
                return result;
        } else {
            return 2; /* pulse coding not allowed for short blocks */
        }
    }

    return 0;
}

/* Table 4.4.25 */
static uint8_t section_data(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld)
{
    uint8_t g;
    uint8_t sect_esc_val, sect_bits;

    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
        sect_bits = 3;
    else
        sect_bits = 5;
    sect_esc_val = (1<<sect_bits) - 1;

#if 0
    printf("\ntotal sfb %d\n", ics->max_sfb);
    printf("   sect    top     cb\n");
#endif

    for (g = 0; g < ics->num_window_groups; g++)
    {
        uint8_t k = 0;
        uint8_t i = 0;

        while (k < ics->max_sfb)
        {
#ifdef ERROR_RESILIENCE
            uint8_t vcb11 = 0;
#endif
            uint8_t sfb;
            uint8_t sect_len_incr;
            uint16_t sect_len = 0;
            uint8_t sect_cb_bits = 4;

            /* if "faad_getbits" detects error and returns "0", "k" is never
               incremented and we cannot leave the while loop */
            if ((ld->error != 0) || (ld->no_more_reading))
                return 14;

#ifdef ERROR_RESILIENCE
            if (hDecoder->aacSectionDataResilienceFlag)
                sect_cb_bits = 5;
#endif

            ics->sect_cb[g][i] = (uint8_t)faad_getbits(ld, sect_cb_bits
                DEBUGVAR(1,71,"section_data(): sect_cb"));

#if 0
            printf("%d\n", ics->sect_cb[g][i]);
#endif

            if (ics->sect_cb[g][i] == NOISE_HCB)
                ics->noise_used = 1;

#ifdef ERROR_RESILIENCE
            if (hDecoder->aacSectionDataResilienceFlag)
            {
                if ((ics->sect_cb[g][i] == 11) ||
                    ((ics->sect_cb[g][i] >= 16) && (ics->sect_cb[g][i] <= 32)))
                {
                    vcb11 = 1;
                }
            }
            if (vcb11)
            {
                sect_len_incr = 1;
            } else {
#endif
                sect_len_incr = (uint8_t)faad_getbits(ld, sect_bits
                    DEBUGVAR(1,72,"section_data(): sect_len_incr"));
#ifdef ERROR_RESILIENCE
            }
#endif
            while ((sect_len_incr == sect_esc_val) /* &&
                (k+sect_len < ics->max_sfb)*/)
            {
                sect_len += sect_len_incr;
                sect_len_incr = (uint8_t)faad_getbits(ld, sect_bits
                    DEBUGVAR(1,72,"section_data(): sect_len_incr"));
            }

            sect_len += sect_len_incr;

            ics->sect_start[g][i] = k;
            ics->sect_end[g][i] = k + sect_len;

#if 0
            printf("%d\n", ics->sect_start[g][i]);
#endif
#if 0
            printf("%d\n", ics->sect_end[g][i]);
#endif

            if (k + sect_len >= 8*15)
                return 15;
            if (i >= 8*15)
                return 15;

            for (sfb = k; sfb < k + sect_len; sfb++)
            {
                ics->sfb_cb[g][sfb] = ics->sect_cb[g][i];
#if 0
                printf("%d\n", ics->sfb_cb[g][sfb]);
#endif
            }

#if 0
            printf(" %6d %6d %6d\n",
                i,
                ics->sect_end[g][i],
                ics->sect_cb[g][i]);
#endif

            k += sect_len;
            i++;
        }
        ics->num_sec[g] = i;
#if 0
        printf("%d\n", ics->num_sec[g]);
#endif
    }

#if 0
    printf("\n");
#endif

    return 0;
}

/*
 *  decode_scale_factors()
 *   decodes the scalefactors from the bitstream
 */
/*
 * All scalefactors (and also the stereo positions and pns energies) are
 * transmitted using Huffman coded DPCM relative to the previous active
 * scalefactor (respectively previous stereo position or previous pns energy,
 * see subclause 4.6.2 and 4.6.3). The first active scalefactor is
 * differentially coded relative to the global gain.
 */
static uint8_t decode_scale_factors(ic_stream *ics, bitfile *ld)
{
    uint8_t g, sfb;
    int16_t t;
    int8_t noise_pcm_flag = 1;

    int16_t scale_factor = ics->global_gain;
    int16_t is_position = 0;
    int16_t noise_energy = ics->global_gain - 90;

    for (g = 0; g < ics->num_window_groups; g++)
    {
        for (sfb = 0; sfb < ics->max_sfb; sfb++)
        {
            switch (ics->sfb_cb[g][sfb])
            {
            case ZERO_HCB: /* zero book */
                ics->scale_factors[g][sfb] = 0;
//#define SF_PRINT
#ifdef SF_PRINT
                printf("%d\n", ics->scale_factors[g][sfb]);
#endif
                break;
            case INTENSITY_HCB: /* intensity books */
            case INTENSITY_HCB2:

                /* decode intensity position */
                t = huffman_scale_factor(ld);
                is_position += (t - 60);
                ics->scale_factors[g][sfb] = is_position;
#ifdef SF_PRINT
                printf("%d\n", ics->scale_factors[g][sfb]);
#endif

                break;
            case NOISE_HCB: /* noise books */

                /* decode noise energy */
                if (noise_pcm_flag)
                {
                    noise_pcm_flag = 0;
                    t = (int16_t)faad_getbits(ld, 9
                        DEBUGVAR(1,73,"scale_factor_data(): first noise")) - 256;
                } else {
                    t = huffman_scale_factor(ld);
                    t -= 60;
                }
                noise_energy += t;
                ics->scale_factors[g][sfb] = noise_energy;
#ifdef SF_PRINT
                printf("%d\n", ics->scale_factors[g][sfb]);
#endif

                break;
            default: /* spectral books */

                /* ics->scale_factors[g][sfb] must be between 0 and 255 */

                ics->scale_factors[g][sfb] = 0;

                /* decode scale factor */
                t = huffman_scale_factor(ld);
                scale_factor += (t - 60);
                if (scale_factor < 0 || scale_factor > 255)
                    return 4;
                ics->scale_factors[g][sfb] = scale_factor;
#ifdef SF_PRINT
                printf("%d\n", ics->scale_factors[g][sfb]);
#endif

                break;
            }
        }
    }

    return 0;
}

/* Table 4.4.26 */
static uint8_t scale_factor_data(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld)
{
    uint8_t ret = 0;
#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif

#ifdef ERROR_RESILIENCE
    if (!hDecoder->aacScalefactorDataResilienceFlag)
    {
#endif
        ret = decode_scale_factors(ics, ld);
#ifdef ERROR_RESILIENCE
    } else {
        /* In ER AAC the parameters for RVLC are seperated from the actual
           data that holds the scale_factors.
           Strangely enough, 2 parameters for HCR are put inbetween them.
        */
        ret = rvlc_scale_factor_data(ics, ld);
    }
#endif

#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->scalefac_cycles += count;
#endif

    return ret;
}

/* Table 4.4.27 */
static void tns_data(ic_stream *ics, tns_info *tns, bitfile *ld)
{
    uint8_t w, filt, i, start_coef_bits, coef_bits;
    uint8_t n_filt_bits = 2;
    uint8_t length_bits = 6;
    uint8_t order_bits = 5;

    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
    {
        n_filt_bits = 1;
        length_bits = 4;
        order_bits = 3;
    }

    for (w = 0; w < ics->num_windows; w++)
    {
        tns->n_filt[w] = (uint8_t)faad_getbits(ld, n_filt_bits
            DEBUGVAR(1,74,"tns_data(): n_filt"));
#if 0
        printf("%d\n", tns->n_filt[w]);
#endif

        if (tns->n_filt[w])
        {
            if ((tns->coef_res[w] = faad_get1bit(ld
                DEBUGVAR(1,75,"tns_data(): coef_res"))) & 1)
            {
                start_coef_bits = 4;
            } else {
                start_coef_bits = 3;
            }
#if 0
            printf("%d\n", tns->coef_res[w]);
#endif
        }

        for (filt = 0; filt < tns->n_filt[w]; filt++)
        {
            tns->length[w][filt] = (uint8_t)faad_getbits(ld, length_bits
                DEBUGVAR(1,76,"tns_data(): length"));
#if 0
            printf("%d\n", tns->length[w][filt]);
#endif
            tns->order[w][filt]  = (uint8_t)faad_getbits(ld, order_bits
                DEBUGVAR(1,77,"tns_data(): order"));
#if 0
            printf("%d\n", tns->order[w][filt]);
#endif
            if (tns->order[w][filt])
            {
                tns->direction[w][filt] = faad_get1bit(ld
                    DEBUGVAR(1,78,"tns_data(): direction"));
#if 0
                printf("%d\n", tns->direction[w][filt]);
#endif
                tns->coef_compress[w][filt] = faad_get1bit(ld
                    DEBUGVAR(1,79,"tns_data(): coef_compress"));
#if 0
                printf("%d\n", tns->coef_compress[w][filt]);
#endif

                coef_bits = start_coef_bits - tns->coef_compress[w][filt];
                for (i = 0; i < tns->order[w][filt]; i++)
                {
                    tns->coef[w][filt][i] = (uint8_t)faad_getbits(ld, coef_bits
                        DEBUGVAR(1,80,"tns_data(): coef"));
#if 0
                    printf("%d\n", tns->coef[w][filt][i]);
#endif
                }
            }
        }
    }
}

#ifdef LTP_DEC
/* Table 4.4.28 */
static uint8_t ltp_data(NeAACDecHandle hDecoder, ic_stream *ics, ltp_info *ltp, bitfile *ld)
{
    uint8_t sfb, w;

    ltp->lag = 0;

#ifdef LD_DEC
    if (hDecoder->object_type == LD)
    {
        ltp->lag_update = (uint8_t)faad_getbits(ld, 1
            DEBUGVAR(1,142,"ltp_data(): lag_update"));

        if (ltp->lag_update)
        {
            ltp->lag = (uint16_t)faad_getbits(ld, 10
                DEBUGVAR(1,81,"ltp_data(): lag"));
        }
    } else {
#endif
        ltp->lag = (uint16_t)faad_getbits(ld, 11
            DEBUGVAR(1,81,"ltp_data(): lag"));
#ifdef LD_DEC
    }
#endif

    /* Check length of lag */
    if (ltp->lag > (hDecoder->frameLength << 1))
        return 18;

    ltp->coef = (uint8_t)faad_getbits(ld, 3
        DEBUGVAR(1,82,"ltp_data(): coef"));

    if (ics->window_sequence == EIGHT_SHORT_SEQUENCE)
    {
        for (w = 0; w < ics->num_windows; w++)
        {
            if ((ltp->short_used[w] = faad_get1bit(ld
                DEBUGVAR(1,83,"ltp_data(): short_used"))) & 1)
            {
                ltp->short_lag_present[w] = faad_get1bit(ld
                    DEBUGVAR(1,84,"ltp_data(): short_lag_present"));
                if (ltp->short_lag_present[w])
                {
                    ltp->short_lag[w] = (uint8_t)faad_getbits(ld, 4
                        DEBUGVAR(1,85,"ltp_data(): short_lag"));
                }
            }
        }
    } else {
        ltp->last_band = (ics->max_sfb < MAX_LTP_SFB ? ics->max_sfb : MAX_LTP_SFB);

        for (sfb = 0; sfb < ltp->last_band; sfb++)
        {
            ltp->long_used[sfb] = faad_get1bit(ld
                DEBUGVAR(1,86,"ltp_data(): long_used"));
        }
    }

    return 0;
}
#endif

/* Table 4.4.29 */
static uint8_t spectral_data(NeAACDecHandle hDecoder, ic_stream *ics, bitfile *ld,
                             int16_t *spectral_data)
{
    int8_t i;
    uint8_t g;
    uint16_t inc, k, p = 0;
    uint8_t groups = 0;
    uint8_t sect_cb;
    uint8_t result;
    uint16_t nshort = hDecoder->frameLength/8;

#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif

    for(g = 0; g < ics->num_window_groups; g++)
    {
        p = groups*nshort;

        for (i = 0; i < ics->num_sec[g]; i++)
        {
            sect_cb = ics->sect_cb[g][i];

            inc = (sect_cb >= FIRST_PAIR_HCB) ? 2 : 4;

            switch (sect_cb)
            {
            case ZERO_HCB:
            case NOISE_HCB:
            case INTENSITY_HCB:
            case INTENSITY_HCB2:
//#define SD_PRINT
#ifdef SD_PRINT
                {
                    int j;
                    for (j = ics->sect_sfb_offset[g][ics->sect_start[g][i]]; j < ics->sect_sfb_offset[g][ics->sect_end[g][i]]; j++)
                    {
                        printf("%d\n", 0);
                    }
                }
#endif
//#define SFBO_PRINT
#ifdef SFBO_PRINT
                printf("%d\n", ics->sect_sfb_offset[g][ics->sect_start[g][i]]);
#endif
                p += (ics->sect_sfb_offset[g][ics->sect_end[g][i]] -
                    ics->sect_sfb_offset[g][ics->sect_start[g][i]]);
                break;
            default:
#ifdef SFBO_PRINT
                printf("%d\n", ics->sect_sfb_offset[g][ics->sect_start[g][i]]);
#endif
                for (k = ics->sect_sfb_offset[g][ics->sect_start[g][i]];
                     k < ics->sect_sfb_offset[g][ics->sect_end[g][i]]; k += inc)
                {
                    if ((result = huffman_spectral_data(sect_cb, ld, &spectral_data[p])) > 0)
                        return result;
#ifdef SD_PRINT
                    {
                        int j;
                        for (j = p; j < p+inc; j++)
                        {
                            printf("%d\n", spectral_data[j]);
                        }
                    }
#endif
                    p += inc;
                }
                break;
            }
        }
        groups += ics->window_group_length[g];
    }

#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->spectral_cycles += count;
#endif

    return 0;
}

/* Table 4.4.30 */
static uint16_t extension_payload(bitfile *ld, drc_info *drc, uint16_t count)
{
    uint16_t i, n, dataElementLength;
    uint8_t dataElementLengthPart;
    uint8_t align = 4, data_element_version, loopCounter;

    uint8_t extension_type = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,87,"extension_payload(): extension_type"));

    switch (extension_type)
    {
    case EXT_DYNAMIC_RANGE:
        drc->present = 1;
        n = dynamic_range_info(ld, drc);
        return n;
    case EXT_FILL_DATA:
        /* fill_nibble = */ faad_getbits(ld, 4
            DEBUGVAR(1,136,"extension_payload(): fill_nibble")); /* must be ‘0000’ */
        for (i = 0; i < count-1; i++)
        {
            /* fill_byte[i] = */ faad_getbits(ld, 8
                DEBUGVAR(1,88,"extension_payload(): fill_byte")); /* must be ‘10100101’ */
        }
        return count;
    case EXT_DATA_ELEMENT:
        data_element_version = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,400,"extension_payload(): data_element_version"));
        switch (data_element_version)
        {
        case ANC_DATA:
            loopCounter = 0;
            dataElementLength = 0;
            do {
                dataElementLengthPart = (uint8_t)faad_getbits(ld, 8
                    DEBUGVAR(1,401,"extension_payload(): dataElementLengthPart"));
                dataElementLength += dataElementLengthPart;
                loopCounter++;
            } while (dataElementLengthPart == 255);

            for (i = 0; i < dataElementLength; i++)
            {
                /* data_element_byte[i] = */ faad_getbits(ld, 8
                    DEBUGVAR(1,402,"extension_payload(): data_element_byte"));
                return (dataElementLength+loopCounter+1);
            }
        default:
            align = 0;
        }
    case EXT_FIL:
    default:
        faad_getbits(ld, align
            DEBUGVAR(1,88,"extension_payload(): fill_nibble"));
        for (i = 0; i < count-1; i++)
        {
            /* other_bits[i] = */ faad_getbits(ld, 8
               DEBUGVAR(1,89,"extension_payload(): fill_bit"));
        }
        return count;
    }
}

/* Table 4.4.31 */
static uint8_t dynamic_range_info(bitfile *ld, drc_info *drc)
{
    uint8_t i, n = 1;
    uint8_t band_incr;

    drc->num_bands = 1;

    if (faad_get1bit(ld
        DEBUGVAR(1,90,"dynamic_range_info(): has instance_tag")) & 1)
    {
        drc->pce_instance_tag = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,91,"dynamic_range_info(): pce_instance_tag"));
        /* drc->drc_tag_reserved_bits = */ faad_getbits(ld, 4
            DEBUGVAR(1,92,"dynamic_range_info(): drc_tag_reserved_bits"));
        n++;
    }

    drc->excluded_chns_present = faad_get1bit(ld
        DEBUGVAR(1,93,"dynamic_range_info(): excluded_chns_present"));
    if (drc->excluded_chns_present == 1)
    {
        n += excluded_channels(ld, drc);
    }

    if (faad_get1bit(ld
        DEBUGVAR(1,94,"dynamic_range_info(): has bands data")) & 1)
    {
        band_incr = (uint8_t)faad_getbits(ld, 4
            DEBUGVAR(1,95,"dynamic_range_info(): band_incr"));
        /* drc->drc_bands_reserved_bits = */ faad_getbits(ld, 4
            DEBUGVAR(1,96,"dynamic_range_info(): drc_bands_reserved_bits"));
        n++;
        drc->num_bands += band_incr;

        for (i = 0; i < drc->num_bands; i++);
        {
            drc->band_top[i] = (uint8_t)faad_getbits(ld, 8
                DEBUGVAR(1,97,"dynamic_range_info(): band_top"));
            n++;
        }
    }

    if (faad_get1bit(ld
        DEBUGVAR(1,98,"dynamic_range_info(): has prog_ref_level")) & 1)
    {
        drc->prog_ref_level = (uint8_t)faad_getbits(ld, 7
            DEBUGVAR(1,99,"dynamic_range_info(): prog_ref_level"));
        /* drc->prog_ref_level_reserved_bits = */ faad_get1bit(ld
            DEBUGVAR(1,100,"dynamic_range_info(): prog_ref_level_reserved_bits"));
        n++;
    }

    for (i = 0; i < drc->num_bands; i++)
    {
        drc->dyn_rng_sgn[i] = faad_get1bit(ld
            DEBUGVAR(1,101,"dynamic_range_info(): dyn_rng_sgn"));
        drc->dyn_rng_ctl[i] = (uint8_t)faad_getbits(ld, 7
            DEBUGVAR(1,102,"dynamic_range_info(): dyn_rng_ctl"));
        n++;
    }

    return n;
}

/* Table 4.4.32 */
static uint8_t excluded_channels(bitfile *ld, drc_info *drc)
{
    uint8_t i, n = 0;
    uint8_t num_excl_chan = 7;

    for (i = 0; i < 7; i++)
    {
        drc->exclude_mask[i] = faad_get1bit(ld
            DEBUGVAR(1,103,"excluded_channels(): exclude_mask"));
    }
    n++;

    while ((drc->additional_excluded_chns[n-1] = faad_get1bit(ld
        DEBUGVAR(1,104,"excluded_channels(): additional_excluded_chns"))) == 1)
    {
        for (i = num_excl_chan; i < num_excl_chan+7; i++)
        {
            drc->exclude_mask[i] = faad_get1bit(ld
                DEBUGVAR(1,105,"excluded_channels(): exclude_mask"));
        }
        n++;
        num_excl_chan += 7;
    }

    return n;
}

/* Annex A: Audio Interchange Formats */

/* Table 1.A.2 */
void get_adif_header(adif_header *adif, bitfile *ld)
{
    uint8_t i;

    /* adif_id[0] = */ faad_getbits(ld, 8
        DEBUGVAR(1,106,"get_adif_header(): adif_id[0]"));
    /* adif_id[1] = */ faad_getbits(ld, 8
        DEBUGVAR(1,107,"get_adif_header(): adif_id[1]"));
    /* adif_id[2] = */ faad_getbits(ld, 8
        DEBUGVAR(1,108,"get_adif_header(): adif_id[2]"));
    /* adif_id[3] = */ faad_getbits(ld, 8
        DEBUGVAR(1,109,"get_adif_header(): adif_id[3]"));
    adif->copyright_id_present = faad_get1bit(ld
        DEBUGVAR(1,110,"get_adif_header(): copyright_id_present"));
    if(adif->copyright_id_present)
    {
        for (i = 0; i < 72/8; i++)
        {
            adif->copyright_id[i] = (int8_t)faad_getbits(ld, 8
                DEBUGVAR(1,111,"get_adif_header(): copyright_id"));
        }
        adif->copyright_id[i] = 0;
    }
    adif->original_copy  = faad_get1bit(ld
        DEBUGVAR(1,112,"get_adif_header(): original_copy"));
    adif->home = faad_get1bit(ld
        DEBUGVAR(1,113,"get_adif_header(): home"));
    adif->bitstream_type = faad_get1bit(ld
        DEBUGVAR(1,114,"get_adif_header(): bitstream_type"));
    adif->bitrate = faad_getbits(ld, 23
        DEBUGVAR(1,115,"get_adif_header(): bitrate"));
    adif->num_program_config_elements = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,116,"get_adif_header(): num_program_config_elements"));

    for (i = 0; i < adif->num_program_config_elements + 1; i++)
    {
        if(adif->bitstream_type == 0)
        {
            adif->adif_buffer_fullness = faad_getbits(ld, 20
                DEBUGVAR(1,117,"get_adif_header(): adif_buffer_fullness"));
        } else {
            adif->adif_buffer_fullness = 0;
        }

        program_config_element(&adif->pce[i], ld);
    }
}

/* Table 1.A.5 */
uint8_t adts_frame(adts_header *adts, bitfile *ld)
{
    /* faad_byte_align(ld); */
    if (adts_fixed_header(adts, ld))
        return 5;
    adts_variable_header(adts, ld);
    adts_error_check(adts, ld);

    return 0;
}

/* Table 1.A.6 */
static uint8_t adts_fixed_header(adts_header *adts, bitfile *ld)
{
    uint16_t i;
    uint8_t sync_err = 1;

    /* try to recover from sync errors */
    for (i = 0; i < 768; i++)
    {
        adts->syncword = (uint16_t)faad_showbits(ld, 12);
        if (adts->syncword != 0xFFF)
        {
            faad_getbits(ld, 8
                DEBUGVAR(0,0,""));
        } else {
            sync_err = 0;
            faad_getbits(ld, 12
                DEBUGVAR(1,118,"adts_fixed_header(): syncword"));
            break;
        }
    }
    if (sync_err)
        return 5;

    adts->id = faad_get1bit(ld
        DEBUGVAR(1,119,"adts_fixed_header(): id"));
    adts->layer = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,120,"adts_fixed_header(): layer"));
    adts->protection_absent = faad_get1bit(ld
        DEBUGVAR(1,121,"adts_fixed_header(): protection_absent"));
    adts->profile = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,122,"adts_fixed_header(): profile"));
    adts->sf_index = (uint8_t)faad_getbits(ld, 4
        DEBUGVAR(1,123,"adts_fixed_header(): sf_index"));
    adts->private_bit = faad_get1bit(ld
        DEBUGVAR(1,124,"adts_fixed_header(): private_bit"));
    adts->channel_configuration = (uint8_t)faad_getbits(ld, 3
        DEBUGVAR(1,125,"adts_fixed_header(): channel_configuration"));
    adts->original = faad_get1bit(ld
        DEBUGVAR(1,126,"adts_fixed_header(): original"));
    adts->home = faad_get1bit(ld
        DEBUGVAR(1,127,"adts_fixed_header(): home"));

    if (adts->old_format == 1)
    {
        /* Removed in corrigendum 14496-3:2002 */
        if (adts->id == 0)
        {
            adts->emphasis = (uint8_t)faad_getbits(ld, 2
                DEBUGVAR(1,128,"adts_fixed_header(): emphasis"));
        }
    }

    return 0;
}

/* Table 1.A.7 */
static void adts_variable_header(adts_header *adts, bitfile *ld)
{
    adts->copyright_identification_bit = faad_get1bit(ld
        DEBUGVAR(1,129,"adts_variable_header(): copyright_identification_bit"));
    adts->copyright_identification_start = faad_get1bit(ld
        DEBUGVAR(1,130,"adts_variable_header(): copyright_identification_start"));
    adts->aac_frame_length = (uint16_t)faad_getbits(ld, 13
        DEBUGVAR(1,131,"adts_variable_header(): aac_frame_length"));
    adts->adts_buffer_fullness = (uint16_t)faad_getbits(ld, 11
        DEBUGVAR(1,132,"adts_variable_header(): adts_buffer_fullness"));
    adts->no_raw_data_blocks_in_frame = (uint8_t)faad_getbits(ld, 2
        DEBUGVAR(1,133,"adts_variable_header(): no_raw_data_blocks_in_frame"));
}

/* Table 1.A.8 */
static void adts_error_check(adts_header *adts, bitfile *ld)
{
    if (adts->protection_absent == 0)
    {
        adts->crc_check = (uint16_t)faad_getbits(ld, 16
            DEBUGVAR(1,134,"adts_error_check(): crc_check"));
    }
}

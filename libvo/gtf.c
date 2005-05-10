/*
 *      Copyright (C) Rudolf Marek <r.marek@sh.cvut.cz> - Aug 2002
 *
 *  You can redistribute this file under terms and conditions
 *  of GNU General Public licence v2.
 * 
 *  GTF calculations formulas are taken from GTF_V1R1.xls
 *  created by ANDY.MORRISH@NSC.COM
 */
       
//Version 0.4
#include "config.h"
#include <stdio.h> 
#include <stdlib.h> 
#include <math.h> 
#include "gtf.h"

#undef GTF_DEBUG

#ifdef GTF_DEBUG
#define DEBUG_PRINTF(a,b) printf(a,b);
#else
#define DEBUG_PRINTF(a,b)
#endif

static GTF_constants GTF_given_constants = { 3.0,550.0,1,8,1.8,8,40,20,128,600 };

#ifndef HAVE_ROUND
static double round(double v) 
{ 
        return floor(v + 0.5); 
} 
#endif
	
static void GetRoundedConstants(GTF_constants *c)
    {
    c->Vsync_need = round(GTF_given_constants.Vsync_need);
    c->min_Vsync_BP = GTF_given_constants.min_Vsync_BP;
    c->min_front_porch = round(GTF_given_constants.min_front_porch);
    c->char_cell_granularity = GTF_given_constants.char_cell_granularity;
    c->margin_width = GTF_given_constants.margin_width;
    c->sync_width = GTF_given_constants.sync_width;
    c->c = ((GTF_given_constants.c - GTF_given_constants.j)*(GTF_given_constants.k / 256)) + GTF_given_constants.j;
    c->j = GTF_given_constants.j;
    c->k = GTF_given_constants.k;
    c->m = (GTF_given_constants.k / 256) * GTF_given_constants.m;
    }

void GTF_calcTimings(double X,double Y,double freq, int type,  
		     int want_margins, int want_interlace,struct VesaCRTCInfoBlock *result )
{
    GTF_constants   c;
    double RR, margin_top, margin_bottom, margin_left, margin_right;
    double estimated_H_period,sync_plus_BP,BP,interlace,V_total_lines_field;
    double estimated_V_field_rate,actual_H_period,actual_V_field_freq;
    double total_active_pixels, ideal_duty_cycle, blanking_time, H_total_pixels;
    double H_freq, pixel_freq,actual_V_frame_freq;
    double H_sync_start, H_sync_end, H_back_porch, H_front_porch, H_sync_width;
    double V_back_porch, V_front_porch, V_sync_start, V_sync_end,V_sync_width;    
    double ideal_H_period;
    GetRoundedConstants(&c);

    
    pixel_freq = RR = freq;

    /* DETERMINE IF 1/2 LINE INTERLACE IS PRESENT */
    
    interlace = 0;    
    
    if (want_interlace) {
    RR = RR * 2;    
    Y=Y/2;
    interlace = 0.5;
    }
    
    result->Flags = 0;
    
    if ((Y==300)||(Y==200)||(Y==240))
    {
    Y*=2;
    result->Flags = VESA_CRTC_DOUBLESCAN; /* TODO: check if mode support   */
    }			
    
    /* DETERMINE NUMBER OF LINES IN V MARGIN */
    /* DETERMINE NUMBER OF PIXELS IN H MARGIN [pixels] */
    
    margin_left = margin_right = 0;
    margin_top = margin_bottom = 0;
    
    if (want_margins) {
	margin_top = margin_bottom = (c.margin_width / 100) * Y;
        margin_left = round(( X* c.margin_width/100)/c.char_cell_granularity) \
		    * c.char_cell_granularity;
    margin_right = margin_left;
    DEBUG_PRINTF("margin_left_right : %f\n",margin_right)
    DEBUG_PRINTF("margin_top_bottom : %f\n",margin_top)
    } 
    
    /* FIND TOTAL NUMBER OF ACTIVE PIXELS (IMAGE + MARGIN) [pixels] */        
    
    total_active_pixels = margin_left + margin_right + X;
    DEBUG_PRINTF("total_active_pixels: %f\n",total_active_pixels)

    if (type == GTF_PF)
    {
    ideal_H_period = ((c.c-100)+(sqrt(((100-c.c)*(100-c.c) )+(0.4*c.m*(total_active_pixels + margin_left + margin_right) / freq))))/2/c.m*1000; 
    
    DEBUG_PRINTF("ideal_H_period: %f\n",ideal_H_period)
    
    /* FIND IDEAL BLANKING DUTY CYCLE FROM FORMULA [%] */
    ideal_duty_cycle = c.c - (c.m * ideal_H_period /1000);
    DEBUG_PRINTF("ideal_duty_cycle: %f\n",ideal_duty_cycle)

    /* FIND BLANKING TIME (TO NEAREST CHAR CELL) [pixels] */
    
    blanking_time = round(total_active_pixels * ideal_duty_cycle \
		    / (100-ideal_duty_cycle) / (2*c.char_cell_granularity))  \
		    * (2*c.char_cell_granularity);
    DEBUG_PRINTF("blanking_time : %f\n",blanking_time )

    /* FIND TOTAL NUMBER OF PIXELS IN A LINE [pixels] */		    
    H_total_pixels = total_active_pixels + blanking_time ;
    DEBUG_PRINTF("H_total_pixels: %f\n",H_total_pixels)
    H_freq = freq / H_total_pixels * 1000;
    DEBUG_PRINTF("H_freq: %f\n",H_freq)
    actual_H_period = 1000 / H_freq;
    DEBUG_PRINTF("actual_H_period: %f\n",actual_H_period)
    sync_plus_BP = round(H_freq * c.min_Vsync_BP/1000);
//   sync_plus_BP = round( freq / H_total_pixels * c.min_Vsync_BP);
   
    DEBUG_PRINTF("sync_plus_BP: %f\n",sync_plus_BP)

    } else if (type == GTF_VF) 
    {
    
    /* ESTIMATE HORIZ. PERIOD [us] */

    estimated_H_period = (( 1/RR ) - c.min_Vsync_BP/1000000 ) /  (Y + (2 * margin_top) + c.min_front_porch + interlace) * 1000000;
    
    DEBUG_PRINTF("estimated_H_period: %f\n",estimated_H_period)

    /* FIND NUMBER OF LINES IN (SYNC + BACK PORCH) [lines] */
    
    sync_plus_BP = round( c.min_Vsync_BP / estimated_H_period );
    DEBUG_PRINTF("sync_plus_BP: %f\n",sync_plus_BP)

    } else if (type == GTF_HF)
    {
    sync_plus_BP = round(freq * c.min_Vsync_BP/1000);
    DEBUG_PRINTF("sync_plus_BP: %f\n",sync_plus_BP)
    }
    
    
    
    /* FIND TOTAL NUMBER OF LINES IN VERTICAL FIELD */

    V_total_lines_field = sync_plus_BP+interlace+margin_bottom+margin_top+Y+c.min_front_porch;
    DEBUG_PRINTF("V_total_lines_field : %f\n",V_total_lines_field )

    if (type == GTF_VF)
    {
    /* ESTIMATE VERTICAL FIELD RATE [hz] */
    
    estimated_V_field_rate = 1 / estimated_H_period / V_total_lines_field * 1000000;
    DEBUG_PRINTF(" estimated_V_field_rate: %f\n", estimated_V_field_rate)
    /* FIND ACTUAL HORIZONTAL PERIOD [us] */
    
    actual_H_period = estimated_H_period / (RR / estimated_V_field_rate);
    DEBUG_PRINTF("actual_H_period: %f\n",actual_H_period)
    /* FIND ACTUAL VERTICAL FIELD FREQUENCY [Hz] */

    actual_V_field_freq = 1 / actual_H_period / V_total_lines_field * 1000000;
    DEBUG_PRINTF("actual_V_field_freq: %f\n",actual_V_field_freq)

    /* FIND IDEAL BLANKING DUTY CYCLE FROM FORMULA [%] */
    ideal_duty_cycle = c.c - (c.m * actual_H_period /1000);
    DEBUG_PRINTF("ideal_duty_cycle: %f\n",ideal_duty_cycle)
    //if (type == GTF_VF)
    //{
    //moved
    //}
    } else if (type == GTF_HF)
    {
    /* FIND IDEAL BLANKING DUTY CYCLE FROM FORMULA [%] */
    ideal_duty_cycle = c.c - (c.m  / freq);
    DEBUG_PRINTF("ideal_duty_cycle: %f\n",ideal_duty_cycle)
    }

    /* FIND BLANKING TIME (TO NEAREST CHAR CELL) [pixels] */

    if (!(type == GTF_PF))
    {
    blanking_time = round(total_active_pixels * ideal_duty_cycle \
		    / (100-ideal_duty_cycle) / (2*c.char_cell_granularity))  \
		    * (2*c.char_cell_granularity);
    DEBUG_PRINTF("blanking_time : %f\n",blanking_time )
    }
    else
//    if (type == GTF_PF)
    {
    actual_V_field_freq = H_freq / V_total_lines_field * 1000;
    }

    if (type == GTF_HF)
    {
    /* Hz */
    actual_V_field_freq = freq / V_total_lines_field * 1000;
    DEBUG_PRINTF("actual_V_field_freq: %f\n",actual_V_field_freq)    
    }
    
        
    actual_V_frame_freq = actual_V_field_freq;

    /* FIND ACTUAL VERTICAL  FRAME FREQUENCY [Hz]*/
    
    if (want_interlace) actual_V_frame_freq = actual_V_field_freq / 2;
    DEBUG_PRINTF("actual_V_frame_freq: %f\n",actual_V_frame_freq)
    
//    V_freq = actual_V_frame_freq;
//    DEBUG_PRINTF("V_freq %f\n",V_freq)    

    
    if (!(type == GTF_PF))
    {    
    /* FIND TOTAL NUMBER OF PIXELS IN A LINE [pixels] */		    
    H_total_pixels = total_active_pixels + blanking_time ;
    DEBUG_PRINTF("H_total_pixels: %f\n",H_total_pixels)
        if (type == GTF_VF)
	{
	/* FIND PIXEL FREQUENCY [Mhz] */
	pixel_freq = H_total_pixels / actual_H_period ;
	DEBUG_PRINTF("pixel_freq: %f\n",pixel_freq)
	} else if (type == GTF_HF)
	{
	/* FIND PIXEL FREQUENCY [Mhz] */
	pixel_freq = H_total_pixels * freq / 1000 ;
	DEBUG_PRINTF("pixel_freq: %f\n",pixel_freq)
	actual_H_period = 1000/freq;
	}

        /* FIND ACTUAL HORIZONTAL FREQUENCY [KHz] */
    
        H_freq = 1000 / actual_H_period;
	DEBUG_PRINTF("H_freq %f\n",H_freq)

    
    }

    /* FIND NUMBER OF LINES IN BACK PORCH [lines] */
    
    BP = sync_plus_BP - c.Vsync_need;
    DEBUG_PRINTF("BP: %f\n",BP)

/*------------------------------------------------------------------------------------------------*/
    /* FIND H SYNC WIDTH (TO NEAREST CHAR CELL) */
    H_sync_width = round(c.sync_width/100*H_total_pixels/c.char_cell_granularity)*c.char_cell_granularity;
    DEBUG_PRINTF("H_sync_width %f\n",H_sync_width)

    /* FIND FRONT H PORCH(TO NEAREST CHAR CELL) */
    H_front_porch = (blanking_time/2) - H_sync_width;
    DEBUG_PRINTF("H_front_porch %f\n",H_front_porch)    
    /* FIND BACK H PORCH(TO NEAREST CHAR CELL) */
    H_back_porch = H_sync_width + H_front_porch;
    DEBUG_PRINTF("H_back_porch%f\n",H_back_porch)  

    H_sync_start = H_total_pixels  - (H_sync_width + H_back_porch);
    DEBUG_PRINTF("H_sync_start %f\n",H_sync_start)
    H_sync_end = H_total_pixels  - H_back_porch;
    DEBUG_PRINTF("H_sync_end %f\n",H_sync_end) 
    
    V_back_porch = interlace + BP;
    DEBUG_PRINTF("V_back_porch%f\n",V_back_porch)
    V_front_porch = interlace + c.min_front_porch;
    DEBUG_PRINTF("V_front_porch%f\n",V_front_porch)   
    
    V_sync_width = c.Vsync_need;
    V_sync_start = V_total_lines_field  - (V_sync_width + V_back_porch);
    DEBUG_PRINTF("V_sync_start %f\n",V_sync_start)    
    V_sync_end = V_total_lines_field  - V_back_porch;
    DEBUG_PRINTF("V_sync_end %f\n",V_sync_end) 
    
    result->hTotal = H_total_pixels;
    result-> hSyncStart  = H_sync_start;  /* Horizontal sync start in pixels */
    result-> hSyncEnd = H_sync_end;   /* Horizontal sync end in pixels */
    result-> vTotal= V_total_lines_field;     /* Vertical total in lines */
    result-> vSyncStart = V_sync_start; /* Vertical sync start in lines */
    result-> vSyncEnd = V_sync_end;   /* Vertical sync end in lines */
    result->  Flags = (result->Flags)|VESA_CRTC_HSYNC_NEG;      /* Flags (Interlaced, Double Scan etc) */
    
    if (want_interlace) 
    {
    result->Flags = (result->Flags) | VESA_CRTC_INTERLACED;
    }
   
    result->  PixelClock = pixel_freq*1000000; /* Pixel clock in units of Hz */
    result-> RefreshRate = actual_V_frame_freq*100;/* Refresh rate in units of 0.01 Hz*/
					
    }



/*
 *  divx4_vbr.c
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of the Linux
 *  distribution for more details.
 *
 *  2-pass code OpenDivX port:
 *  Copyright (C) 2001 Christoph Lampert <gruel@gmx.de> 
 *
 *  Large parts of this code were taken from VbrControl() from the OpenDivX
 *  project, (C) divxnetworks, written by Eugene Kuznetsov <ekuznetsov@divxnetworks.com>
 *  with the permission of Darrius "Junto" Thompson, Director DivX
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <inttypes.h>

#include "divx4_vbr.h"

//#include "transcode.h"

#define FALSE 0
#define TRUE 1

/*  Absolute maximum and minimum quantizers used in VBR modes */
static const int min_quantizer=1;
static const int max_quantizer=31;

/*  Limits on frame-level deviation of quantizer ( higher values
	correspond to frames with more changes and vice versa ) */	
static const float min_quant_delta=-10.f;
static const float max_quant_delta=5.f;
/*  Limits on stream-level deviation of quantizer ( used to make
	overall bitrate of stream close to requested value ) */
static const float min_rc_quant_delta=.6f;
static const float max_rc_quant_delta=1.5f;

/*  Crispness parameter	controls threshold for decision whether
    to skip the frame or to code it. */
//static const float max_crispness=100.f;
/*	Maximum allowed number of skipped frames in a line. */
//static const int max_drops_line=0;					// CHL   We don't drop frames at the moment!


typedef struct entry_s
	/* max 28 bytes/frame or 5 Mb for 2-hour movie */
	{
		int quant;
		int text_bits;
		int motion_bits;
		int total_bits;
		float mult;
		short is_key_frame;
		short drop;
	} entry;

static int m_iCount;
static int m_iQuant;
static int m_iCrispness;
static short m_bDrop;
static float m_fQuant;

static int64_t m_lEncodedBits;
static int64_t m_lExpectedBits;

static FILE  *m_pFile;

static entry    vFrame;
static entry *m_vFrames;
static long	lFrameStart;

static int	iNumFrames;
static int	dummy;


void VbrControl_init_1pass_vbr(int quality, int crispness)
{
  m_fQuant=min_quantizer+((max_quantizer-min_quantizer)/6.)*(6-quality);
  m_iCount=0;
  m_bDrop=FALSE;
  VbrControl_update_1pass_vbr();
}

int VbrControl_init_2pass_vbr_analysis(const char *filename, int quality)
{
	m_pFile=fopen(filename, "wb");
	if(m_pFile==0)
		return -1;
	m_iCount=0;
	m_bDrop=FALSE;
	fprintf(m_pFile, "##version 1\n");
	fprintf(m_pFile, "quality %d\n", quality);
	return 0;
}

int VbrControl_init_2pass_vbr_encoding(const char *filename, int bitrate, double framerate, int crispness, int quality)
{
	int i;
	
	int64_t text_bits=0;
	int64_t total_bits=0;
	int64_t complexity=0;
	int64_t new_complexity=0;
	int64_t motion_bits=0;
	int64_t denominator=0;
	float qual_multiplier=1.;
	char head[20];

	int64_t desired_bits;
	int64_t non_text_bits;

	float average_complexity;

	m_pFile=fopen(filename, "rb");
	if(m_pFile==0)
		return -1;
	m_bDrop=FALSE;
	m_iCount=0;

	fread(head, 10, 1, m_pFile);
	if(!strncmp("##version ", head, 10))
	{
	    int version;
	    int iOldQual;
	    float old_qual, new_qual;
	    fscanf(m_pFile, "%d\n", &version);
	    fscanf(m_pFile, "quality %d\n", &iOldQual);
	    switch(iOldQual)
	    {
	    case 5:
		old_qual=1.f;
		break;
	    case 4:
		old_qual=1.1f;
		break;
	    case 3:
		old_qual=1.25f;
		break;
	    case 2:
		old_qual=1.4f;
		break;
	    case 1:
		old_qual=2.f;
		break;
	    }
	    switch(quality)
	    {
	    case 5:
		new_qual=1.f;
		break;
	    case 4:
		new_qual=1.1f;
		break;
	    case 3:
		new_qual=1.25f;
		break;
	    case 2:
		new_qual=1.4f;
		break;
	    case 1:
		new_qual=2.f;
		break;
	    }
	    qual_multiplier=new_qual/old_qual;	
	}
	else
	    fseek(m_pFile, 0, SEEK_SET);

	lFrameStart=ftell(m_pFile);		// save current position	

/* removed C++ dependencies, now read file twice :-( */

	
		while(!feof(m_pFile)) 
      {  fscanf(m_pFile, "Frame %d: intra %d, quant %d, texture %d, motion %d, total %d\n",
         &iNumFrames, &(vFrame.is_key_frame), &(vFrame.quant), &(vFrame.text_bits), &(vFrame.motion_bits), &(vFrame.total_bits));

                vFrame.total_bits+=vFrame.text_bits*(qual_multiplier-1);
                vFrame.text_bits*=qual_multiplier;
                text_bits +=(int64_t)vFrame.text_bits;
					 motion_bits += (int64_t)vFrame.motion_bits;
                total_bits +=(int64_t)vFrame.total_bits;
                complexity +=(int64_t)vFrame.text_bits*vFrame.quant;

//	printf("Frames %d, texture %d, motion %d, quant %d total %d ",
//		iNumFrames, vFrame.text_bits, vFrame.motion_bits, vFrame.quant, vFrame.total_bits);
//	printf("texture %d, total %d, complexity %lld \n",vFrame.text_bits,vFrame.total_bits, complexity);
	 	}
		iNumFrames++;
		average_complexity=complexity/iNumFrames;
		
//		if (verbose & TC_DEBUG)	{
//		    fprintf(stderr, "(%s) frames %d, texture %lld, motion %lld, total %lld, complexity %lld\n", __FILE__, iNumFrames, text_bits, motion_bits, total_bits, complexity);
//		}
		
		m_vFrames = (entry*)malloc(iNumFrames*sizeof(entry));
		if (!m_vFrames) 
		{	printf("out of memory");
			return -2; //TC_EXPORT_ERROR;
		}
			
	   fseek(m_pFile, lFrameStart, SEEK_SET);		// start again
		
		for (i=0;i<iNumFrames;i++)
		{  fscanf(m_pFile, "Frame %d: intra %d, quant %d, texture %d, motion %d, total %d\n",
         &dummy, &(m_vFrames[i].is_key_frame), &(m_vFrames[i].quant), 
			&(m_vFrames[i].text_bits), &(m_vFrames[i].motion_bits), 
			&(m_vFrames[i].total_bits));

			m_vFrames[i].total_bits += m_vFrames[i].text_bits*(qual_multiplier-1);
         m_vFrames[i].text_bits  *= qual_multiplier;
	 	}

	if (m_pFile)
		{	fclose(m_pFile);
			m_pFile=NULL;
		}

	desired_bits=(int64_t)bitrate*(int64_t)iNumFrames/framerate;
	non_text_bits=total_bits-text_bits;

	if(desired_bits<=non_text_bits)
	{
		char s[200];
		printf("Specified bitrate is too low for this clip.\n"
			"Minimum possible bitrate for the clip is %.0f kbps. Overriding\n"
			"user-specified value.\n",
				(float)(non_text_bits*framerate/(int64_t)iNumFrames));

		desired_bits=non_text_bits*3/2;
/*
		m_fQuant=max_quantizer;
		for(int i=0; i<iNumFrames; i++)
		{
			m_vFrames[i].drop=0;
			m_vFrames[i].mult=1;
		}
		VbrControl_set_quant(m_fQuant);
		return 0;
*/
  }

	desired_bits -= non_text_bits;
		/**
		BRIEF EXPLANATION OF WHAT'S GOING ON HERE.
		We assume that 
			text_bits=complexity / quantizer
			total_bits-text_bits = const(complexity)
		where 'complexity' is a characteristic of the frame
		and does not depend much on quantizer dynamics.
		Using this equation, we calculate 'average' quantizer
		to be used for encoding ( 1st order effect ).
		Having constant quantizer for the entire stream is not
		very convenient - reconstruction errors are
		more noticeable in low-motion scenes. To compensate 
		this effect, we multiply quantizer for each frame by
			(complexity/average_complexity)^k,
		( k - parameter of adjustment ). k=0 means 'no compensation'
		and k=1 is 'constant bitrate mode'. We choose something in
		between, like 0.5 ( 2nd order effect ).
		**/
		
	average_complexity=complexity/iNumFrames;

	for(i=0; i<iNumFrames; i++)
	{
		float mult;
		if(m_vFrames[i].is_key_frame)
		{
		    if((i+1<iNumFrames) && (m_vFrames[i+1].is_key_frame))
			mult=1.25;
		    else
			mult=.75;
		}
		else
		{
			mult=m_vFrames[i].text_bits*m_vFrames[i].quant;
			mult=(float)sqrt(mult/average_complexity);
			
//			if(i && m_vFrames[i-1].is_key_frame)
//			    mult *= 0.75;
			if(mult<0.5)
			    mult=0.5;
			if(mult>1.5)
			    mult=1.5;
		}

		m_vFrames[i].mult=mult;
		m_vFrames[i].drop=FALSE;
		new_complexity+=m_vFrames[i].text_bits*m_vFrames[i].quant;

		denominator+=desired_bits*m_vFrames[i].mult/iNumFrames;
	}
	
	m_fQuant=((double)new_complexity)/(double)denominator;

	if(m_fQuant<min_quantizer) m_fQuant=min_quantizer;
	if(m_fQuant>max_quantizer) m_fQuant=max_quantizer;
	m_pFile=fopen("analyse.log", "wb");
	if(m_pFile)
	{
		fprintf(m_pFile, "Total frames: %d Avg quantizer: %f\n",
			iNumFrames, m_fQuant);
		fprintf(m_pFile, "Expecting %12lld bits\n", desired_bits+non_text_bits);
		fflush(m_pFile);
	}
	VbrControl_set_quant(m_fQuant*m_vFrames[0].mult);
	m_lEncodedBits=m_lExpectedBits=0;
	return 0;
}

int VbrControl_get_intra() 
{
	return m_vFrames[m_iCount].is_key_frame;
}

short VbrControl_get_drop() 
{
	return m_bDrop;
}

int VbrControl_get_quant() 
{
	return m_iQuant;
}

void VbrControl_set_quant(float quant)
{
	m_iQuant=quant;
	if((rand() % 10)<((quant-m_iQuant) * 10))
		m_iQuant++;
	if(m_iQuant<min_quantizer) m_iQuant=min_quantizer;
	if(m_iQuant>max_quantizer) m_iQuant=max_quantizer;
}

void VbrControl_update_1pass_vbr()
{
	VbrControl_set_quant(m_fQuant);
	m_iCount++;
}

void VbrControl_update_2pass_vbr_analysis(int is_key_frame, int motion_bits, int texture_bits, int total_bits, int quant)
{
	if(!m_pFile)
		return;
	fprintf(m_pFile, "Frame %d: intra %d, quant %d, texture %d, motion %d, total %d\n",
		m_iCount, is_key_frame, quant, texture_bits, motion_bits, total_bits);
	m_iCount++;
}

void VbrControl_update_2pass_vbr_encoding(int motion_bits, int texture_bits, int total_bits)
{
	double q;
	double dq;
		
	if(m_iCount>=iNumFrames)
		return;

	m_lExpectedBits+=(m_vFrames[m_iCount].total_bits-m_vFrames[m_iCount].text_bits)
		+ m_vFrames[m_iCount].text_bits*m_vFrames[m_iCount].quant/m_fQuant;
	m_lEncodedBits+=(int64_t)total_bits;

	if(m_pFile)
		fprintf(m_pFile, "Frame %d: PRESENT, complexity %d, quant multiplier %f, texture %d, total %d ",
				m_iCount, m_vFrames[m_iCount].text_bits*m_vFrames[m_iCount].quant,
					m_vFrames[m_iCount].mult, texture_bits, total_bits);

	m_iCount++;

	q = m_fQuant * m_vFrames[m_iCount].mult;
	if(q<m_fQuant+min_quant_delta) q=m_fQuant+min_quant_delta;
	if(q>m_fQuant+max_quant_delta) q=m_fQuant+max_quant_delta;

	dq = (double)m_lEncodedBits/(double)m_lExpectedBits;
	dq*=dq;
	if(dq<min_rc_quant_delta) 
		dq=min_rc_quant_delta;
	if(dq>max_rc_quant_delta) 
		dq=max_rc_quant_delta;
	if(m_iCount<20)					// no framerate corrections in first frames 
		dq=1;
	if(m_pFile)
		fprintf(m_pFile, "Progress: expected %12lld, achieved %12lld, dq %f",
			m_lExpectedBits, m_lEncodedBits, dq);
	q *= dq;
	VbrControl_set_quant(q);
	if(m_pFile)
		fprintf(m_pFile, ", new quant %d\n", m_iQuant);
}

void VbrControl_close()
{
	if(m_pFile)
	{
		fclose(m_pFile);
		m_pFile=NULL;
	}
	free(m_vFrames);
}

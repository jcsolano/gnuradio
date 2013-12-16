/* -*- c++ -*- */
/*
 * Copyright 2006,2010, 2013 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnuradio/atsc/fpll.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/atsc/consts.h>
#include <algorithm>
#include <gnuradio/math.h>
#include <iostream>

atsc_fpll_sptr
atsc_make_fpll( float sample_rate )
{
	return gnuradio::get_initial_sptr(new atsc_fpll( sample_rate ));
}

atsc_fpll::atsc_fpll( float sample_rate )
	: gr::sync_block("atsc_fpll",
		gr::io_signature::make(1, 1, sizeof(gr_complex)),
		gr::io_signature::make(1, 1, sizeof(gr_complex))),
		initial_phase(0)
{
	initial_freq = -3e6 + 0.309e6; // a_initial_freq;
	initialize( sample_rate );
}

void
atsc_fpll::initialize ( float sample_rate )
{
	float alpha = 1 - exp(-1.0 / sample_rate / 5e-6);

	afc.set_taps (alpha);

	printf("Setting initial_freq: %f\n",initial_freq);
	nco.set_freq (initial_freq / sample_rate * 2 * M_PI);
	nco.set_phase (initial_phase);
}

int
atsc_fpll::work (int noutput_items,
		       gr_vector_const_void_star &input_items,
		       gr_vector_void_star &output_items)
{
	const gr_complex *in = (const gr_complex *) input_items[0];
	gr_complex *out = (gr_complex *) output_items[0];

	for (int k = 0; k < noutput_items; k++)
	{
		float a_cos, a_sin;

		nco.step ();                // increment phase
		nco.sincos (&a_sin, &a_cos);  // compute cos and sin

		gr_complex result = in[k] * gr_complex(a_sin, a_cos);

    		out[k] = result;

		gr_complex filtered = afc.filter (result);

		// phase detector

		// float x = atan2 (filtered_Q, filtered_I);
		float x = gr::fast_atan2f(filtered.imag(), filtered.real());

		// avoid slamming filter with big transitions

		static const float limit = M_PI / 2;

		if (x > limit)
			x = limit;
		else if (x < -limit)
			x = -limit;

		// static const float alpha = 0.037;   // Max value
		// static const float alpha = 0.005;   // takes about 5k samples to pull in, stddev = 323
		// static const float alpha = 0.002;   // takes about 15k samples to pull in, stddev =  69
                                           //  or about 120k samples on noisy data,
		static const float alpha = 0.0002;
		static const float beta = alpha * alpha / 4;

		nco.adjust_phase (alpha * x);
		nco.adjust_freq (beta * x);
	}

	return noutput_items;
}



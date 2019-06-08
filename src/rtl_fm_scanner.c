/*
 * rtl_fm_scanner - a command-line based scanner for rtl sdr.
 * fork from rtl-sdr's  receiver.
 * This code is forked from rtl_fm.c from the rtl-sdr project which is GPL.
 * Modified by Alex Couture-Beil <rtl-scanner@mofo.ca>
 *
 **************************************************************************
 * Original rtl-sdr copyright:
 **************************************************************************
 *
 * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
 * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
 * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
 * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
 * Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <math.h>
#include <pthread.h>
#include <stdbool.h>

#include <curses.h>
#include <pulse/error.h>
#include <pulse/simple.h>

#include "rtl-sdr.h"

pa_simple* pa_handle;

double atofs( char* s )
/* standard suffixes */
{
	char last;
	int len;
	double suff = 1.0;
	len = strlen( s );
	last = s[len - 1];
	s[len - 1] = '\0';
	switch( last ) {
	case 'g':
	case 'G':
		suff *= 1e3;
		/* fall-through */
	case 'm':
	case 'M':
		suff *= 1e3;
		/* fall-through */
	case 'k':
	case 'K':
		suff *= 1e3;
		suff *= atof( s );
		s[len - 1] = last;
		return suff;
	}
	s[len - 1] = last;
	return atof( s );
}

double atoft( char* s )
/* time suffixes, returns seconds */
{
	char last;
	int len;
	double suff = 1.0;
	len = strlen( s );
	last = s[len - 1];
	s[len - 1] = '\0';
	switch( last ) {
	case 'h':
	case 'H':
		suff *= 60;
		/* fall-through */
	case 'm':
	case 'M':
		suff *= 60;
		/* fall-through */
	case 's':
	case 'S':
		suff *= atof( s );
		s[len - 1] = last;
		return suff;
	}
	s[len - 1] = last;
	return atof( s );
}

double atofp( char* s )
/* percent suffixes */
{
	char last;
	int len;
	double suff = 1.0;
	len = strlen( s );
	last = s[len - 1];
	s[len - 1] = '\0';
	switch( last ) {
	case '%':
		suff *= 0.01;
		suff *= atof( s );
		s[len - 1] = last;
		return suff;
	}
	s[len - 1] = last;
	return atof( s );
}

int nearest_gain( rtlsdr_dev_t* dev, int target_gain )
{
	int i, r, err1, err2, count, nearest;
	int* gains;
	r = rtlsdr_set_tuner_gain_mode( dev, 1 );
	if( r < 0 ) {
		fprintf( stderr, "WARNING: Failed to enable manual gain.\n" );
		return r;
	}
	count = rtlsdr_get_tuner_gains( dev, NULL );
	if( count <= 0 ) {
		return 0;
	}
	gains = malloc( sizeof( int ) * count );
	count = rtlsdr_get_tuner_gains( dev, gains );
	nearest = gains[0];
	for( i = 0; i < count; i++ ) {
		err1 = abs( target_gain - nearest );
		err2 = abs( target_gain - gains[i] );
		if( err2 < err1 ) {
			nearest = gains[i];
		}
	}
	free( gains );
	return nearest;
}

int verbose_set_frequency( rtlsdr_dev_t* dev, uint32_t frequency )
{
	int r;
	r = rtlsdr_set_center_freq( dev, frequency );
	if( r < 0 ) {
		fprintf( stderr, "WARNING: Failed to set center freq.\n" );
	}
	else {
		fprintf( stderr, "Tuned to %u Hz.\n", frequency );
	}
	return r;
}

int verbose_set_sample_rate( rtlsdr_dev_t* dev, uint32_t samp_rate )
{
	int r;
	r = rtlsdr_set_sample_rate( dev, samp_rate );
	if( r < 0 ) {
		fprintf( stderr, "WARNING: Failed to set sample rate.\n" );
	}
	else {
		fprintf( stderr, "Sampling at %u S/s.\n", samp_rate );
	}
	return r;
}

int verbose_direct_sampling( rtlsdr_dev_t* dev, int on )
{
	int r;
	r = rtlsdr_set_direct_sampling( dev, on );
	if( r != 0 ) {
		fprintf( stderr, "WARNING: Failed to set direct sampling mode.\n" );
		return r;
	}
	if( on == 0 ) {
		fprintf( stderr, "Direct sampling mode disabled.\n" );
	}
	if( on == 1 ) {
		fprintf( stderr, "Enabled direct sampling mode, input 1/I.\n" );
	}
	if( on == 2 ) {
		fprintf( stderr, "Enabled direct sampling mode, input 2/Q.\n" );
	}
	return r;
}

int verbose_offset_tuning( rtlsdr_dev_t* dev )
{
	int r;
	r = rtlsdr_set_offset_tuning( dev, 1 );
	if( r != 0 ) {
		fprintf( stderr, "WARNING: Failed to set offset tuning.\n" );
	}
	else {
		fprintf( stderr, "Offset tuning mode enabled.\n" );
	}
	return r;
}

int verbose_auto_gain( rtlsdr_dev_t* dev )
{
	int r;
	r = rtlsdr_set_tuner_gain_mode( dev, 0 );
	if( r != 0 ) {
		fprintf( stderr, "WARNING: Failed to set tuner gain.\n" );
	}
	else {
		fprintf( stderr, "Tuner gain set to automatic.\n" );
	}
	return r;
}

int verbose_gain_set( rtlsdr_dev_t* dev, int gain )
{
	int r;
	r = rtlsdr_set_tuner_gain_mode( dev, 1 );
	if( r < 0 ) {
		fprintf( stderr, "WARNING: Failed to enable manual gain.\n" );
		return r;
	}
	r = rtlsdr_set_tuner_gain( dev, gain );
	if( r != 0 ) {
		fprintf( stderr, "WARNING: Failed to set tuner gain.\n" );
	}
	else {
		fprintf( stderr, "Tuner gain set to %0.2f dB.\n", gain / 10.0 );
	}
	return r;
}

int verbose_ppm_set( rtlsdr_dev_t* dev, int ppm_error )
{
	int r;
	if( ppm_error == 0 ) {
		return 0;
	}
	r = rtlsdr_set_freq_correction( dev, ppm_error );
	if( r < 0 ) {
		fprintf( stderr, "WARNING: Failed to set ppm error.\n" );
	}
	else {
		fprintf( stderr, "Tuner error set to %i ppm.\n", ppm_error );
	}
	return r;
}

int verbose_reset_buffer( rtlsdr_dev_t* dev )
{
	int r;
	r = rtlsdr_reset_buffer( dev );
	if( r < 0 ) {
		fprintf( stderr, "WARNING: Failed to reset buffers.\n" );
	}
	return r;
}

int verbose_device_search( char* s )
{
	int i, device_count, device, offset;
	char* s2;
	char vendor[256], product[256], serial[256];
	device_count = rtlsdr_get_device_count();
	if( !device_count ) {
		fprintf( stderr, "No supported devices found.\n" );
		return -1;
	}
	fprintf( stderr, "Found %d device(s):\n", device_count );
	for( i = 0; i < device_count; i++ ) {
		rtlsdr_get_device_usb_strings( i, vendor, product, serial );
		fprintf( stderr, "  %d:  %s, %s, SN: %s\n", i, vendor, product, serial );
	}
	fprintf( stderr, "\n" );
	/* does string look like raw id number */
	device = (int)strtol( s, &s2, 0 );
	if( s2[0] == '\0' && device >= 0 && device < device_count ) {
		fprintf(
			stderr, "Using device %d: %s\n", device, rtlsdr_get_device_name( (uint32_t)device ) );
		return device;
	}
	/* does string exact match a serial */
	for( i = 0; i < device_count; i++ ) {
		rtlsdr_get_device_usb_strings( i, vendor, product, serial );
		if( strcmp( s, serial ) != 0 ) {
			continue;
		}
		device = i;
		fprintf(
			stderr, "Using device %d: %s\n", device, rtlsdr_get_device_name( (uint32_t)device ) );
		return device;
	}
	/* does string prefix match a serial */
	for( i = 0; i < device_count; i++ ) {
		rtlsdr_get_device_usb_strings( i, vendor, product, serial );
		if( strncmp( s, serial, strlen( s ) ) != 0 ) {
			continue;
		}
		device = i;
		fprintf(
			stderr, "Using device %d: %s\n", device, rtlsdr_get_device_name( (uint32_t)device ) );
		return device;
	}
	/* does string suffix match a serial */
	for( i = 0; i < device_count; i++ ) {
		rtlsdr_get_device_usb_strings( i, vendor, product, serial );
		offset = strlen( serial ) - strlen( s );
		if( offset < 0 ) {
			continue;
		}
		if( strncmp( s, serial + offset, strlen( s ) ) != 0 ) {
			continue;
		}
		device = i;
		fprintf(
			stderr, "Using device %d: %s\n", device, rtlsdr_get_device_name( (uint32_t)device ) );
		return device;
	}
	fprintf( stderr, "No matching devices found.\n" );
	return -1;
}

#define DEFAULT_SAMPLE_RATE 24000
#define DEFAULT_BUF_LENGTH ( 1 * 16384 )
#define MAXIMUM_OVERSAMPLE 16
#define MAXIMUM_BUF_LENGTH ( MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH )
#define AUTO_GAIN -100
#define BUFFER_DUMP 4096

#define FREQUENCIES_LIMIT 1000

static volatile int do_exit = 0;
static int lcm_post[17] = {1, 1, 1, 3, 1, 5, 3, 7, 1, 9, 5, 11, 3, 13, 7, 15, 1};
static int ACTUAL_BUF_LENGTH;

static int* atan_lut = NULL;
static int atan_lut_size = 131072; /* 512 KB */
static int atan_lut_coef = 8;

struct dongle_state
{
	int exit_flag;
	pthread_t thread;
	rtlsdr_dev_t* dev;
	int dev_index;
	uint32_t freq;
	uint32_t rate;
	int gain;
	uint16_t buf16[MAXIMUM_BUF_LENGTH];
	uint32_t buf_len;
	int ppm_error;
	int offset_tuning;
	int direct_sampling;
	int mute;
	struct demod_state* demod_target;
};

struct demod_state
{
	int exit_flag;
	pthread_t thread;
	int16_t lowpassed[MAXIMUM_BUF_LENGTH];
	int lp_len;
	int16_t lp_i_hist[10][6];
	int16_t lp_q_hist[10][6];
	int16_t result[MAXIMUM_BUF_LENGTH];
	int16_t droop_i_hist[9];
	int16_t droop_q_hist[9];
	int result_len;
	int rate_in;
	int rate_out;
	int rate_out2;
	int now_r, now_j;
	int pre_r, pre_j;
	int prev_index;
	int downsample; /* min 1, max 256 */
	int post_downsample;
	int output_scale;
	int squelch_level;
	int downsample_passes;
	int comp_fir_size;
	int custom_atan;
	int deemph, deemph_a;
	int now_lpr;
	int prev_lpr_index;
	int dc_block, dc_avg;
	void ( *mode_demod )( struct demod_state* );
	pthread_rwlock_t rw;
	pthread_cond_t ready;
	pthread_mutex_t ready_m;
	struct output_state* output_target;

	int signal; // not processed: -1;
};

struct output_state
{
	int exit_flag;
	pthread_t thread;
	FILE* file;
	char* filename;
	int16_t result[MAXIMUM_BUF_LENGTH];
	int result_len;
	int rate;
	pthread_rwlock_t rw;
	pthread_cond_t ready;
	pthread_mutex_t ready_m;
};

struct freq
{
	uint32_t freq;
	int scan_squelch;
	int open_squelch;
	int open_duration;
	int skip_duration;
	char* desc;
};

struct controller_state
{
	int exit_flag;
	pthread_t thread;
	struct freq freqs[FREQUENCIES_LIMIT];
	int freq_len;
	int wb_mode;
	pthread_cond_t hop;
	pthread_mutex_t hop_m;
	bool scanning;
	bool skip;
	int freq_adjust;
};

// multiple of these, eventually
struct dongle_state dongle;
struct demod_state demod;
struct output_state output;
struct controller_state controller;

static void sighandler( int signum )
{
	fprintf( stderr, "Signal caught, exiting!\n" );
	do_exit = 1;
	rtlsdr_cancel_async( dongle.dev );
}

/* more cond dumbness */
#define safe_cond_signal( n, m )                                                                   \
	pthread_mutex_lock( m );                                                                       \
	pthread_cond_signal( n );                                                                      \
	pthread_mutex_unlock( m )
#define safe_cond_wait( n, m )                                                                     \
	pthread_mutex_lock( m );                                                                       \
	pthread_cond_wait( n, m );                                                                     \
	pthread_mutex_unlock( m )

/* {length, coef, coef, coef}  and scaled by 2^15
   for now, only length 9, optimal way to get +85% bandwidth */
#define CIC_TABLE_MAX 10
int cic_9_tables[][10] = {
	{
		0,
	},
	{9, -156, -97, 2798, -15489, 61019, -15489, 2798, -97, -156},
	{9, -128, -568, 5593, -24125, 74126, -24125, 5593, -568, -128},
	{9, -129, -639, 6187, -26281, 77511, -26281, 6187, -639, -129},
	{9, -122, -612, 6082, -26353, 77818, -26353, 6082, -612, -122},
	{9, -120, -602, 6015, -26269, 77757, -26269, 6015, -602, -120},
	{9, -120, -582, 5951, -26128, 77542, -26128, 5951, -582, -120},
	{9, -119, -580, 5931, -26094, 77505, -26094, 5931, -580, -119},
	{9, -119, -578, 5921, -26077, 77484, -26077, 5921, -578, -119},
	{9, -119, -577, 5917, -26067, 77473, -26067, 5917, -577, -119},
	{9, -199, -362, 5303, -25505, 77489, -25505, 5303, -362, -199},
};

void rotate_90( unsigned char* buf, uint32_t len )
/* 90 rotation is 1+0j, 0+1j, -1+0j, 0-1j
   or [0, 1, -3, 2, -4, -5, 7, -6] */
{
	uint32_t i;
	unsigned char tmp;
	for( i = 0; i < len; i += 8 ) {
		/* uint8_t negation = 255 - x */
		tmp = 255 - buf[i + 3];
		buf[i + 3] = buf[i + 2];
		buf[i + 2] = tmp;

		buf[i + 4] = 255 - buf[i + 4];
		buf[i + 5] = 255 - buf[i + 5];

		tmp = 255 - buf[i + 6];
		buf[i + 6] = buf[i + 7];
		buf[i + 7] = tmp;
	}
}

void low_pass( struct demod_state* d )
/* simple square window FIR */
{
	int i = 0, i2 = 0;
	while( i < d->lp_len ) {
		d->now_r += d->lowpassed[i];
		d->now_j += d->lowpassed[i + 1];
		i += 2;
		d->prev_index++;
		if( d->prev_index < d->downsample ) {
			continue;
		}
		d->lowpassed[i2] = d->now_r; // * d->output_scale;
		d->lowpassed[i2 + 1] = d->now_j; // * d->output_scale;
		d->prev_index = 0;
		d->now_r = 0;
		d->now_j = 0;
		i2 += 2;
	}
	d->lp_len = i2;
}

int low_pass_simple( int16_t* signal2, int len, int step )
// no wrap around, length must be multiple of step
{
	int i, i2, sum;
	for( i = 0; i < len; i += step ) {
		sum = 0;
		for( i2 = 0; i2 < step; i2++ ) {
			sum += (int)signal2[i + i2];
		}
		// signal2[i/step] = (int16_t)(sum / step);
		signal2[i / step] = ( int16_t )( sum );
	}
	signal2[i / step + 1] = signal2[i / step];
	return len / step;
}

void low_pass_real( struct demod_state* s )
/* simple square window FIR */
// add support for upsampling?
{
	int i = 0, i2 = 0;
	int fast = (int)s->rate_out;
	int slow = s->rate_out2;
	while( i < s->result_len ) {
		s->now_lpr += s->result[i];
		i++;
		s->prev_lpr_index += slow;
		if( s->prev_lpr_index < fast ) {
			continue;
		}
		s->result[i2] = ( int16_t )( s->now_lpr / ( fast / slow ) );
		s->prev_lpr_index -= fast;
		s->now_lpr = 0;
		i2 += 1;
	}
	s->result_len = i2;
}

void fifth_order( int16_t* data, int length, int16_t* hist )
/* for half of interleaved data */
{
	int i;
	int16_t a, b, c, d, e, f;
	a = hist[1];
	b = hist[2];
	c = hist[3];
	d = hist[4];
	e = hist[5];
	f = data[0];
	/* a downsample should improve resolution, so don't fully shift */
	data[0] = ( a + ( b + e ) * 5 + ( c + d ) * 10 + f ) >> 4;
	for( i = 4; i < length; i += 4 ) {
		a = c;
		b = d;
		c = e;
		d = f;
		e = data[i - 2];
		f = data[i];
		data[i / 2] = ( a + ( b + e ) * 5 + ( c + d ) * 10 + f ) >> 4;
	}
	/* archive */
	hist[0] = a;
	hist[1] = b;
	hist[2] = c;
	hist[3] = d;
	hist[4] = e;
	hist[5] = f;
}

void generic_fir( int16_t* data, int length, int* fir, int16_t* hist )
/* Okay, not at all generic.  Assumes length 9, fix that eventually. */
{
	int d, temp, sum;
	for( d = 0; d < length; d += 2 ) {
		temp = data[d];
		sum = 0;
		sum += ( hist[0] + hist[8] ) * fir[1];
		sum += ( hist[1] + hist[7] ) * fir[2];
		sum += ( hist[2] + hist[6] ) * fir[3];
		sum += ( hist[3] + hist[5] ) * fir[4];
		sum += hist[4] * fir[5];
		data[d] = sum >> 15;
		hist[0] = hist[1];
		hist[1] = hist[2];
		hist[2] = hist[3];
		hist[3] = hist[4];
		hist[4] = hist[5];
		hist[5] = hist[6];
		hist[6] = hist[7];
		hist[7] = hist[8];
		hist[8] = temp;
	}
}

/* define our own complex math ops
   because ARMv5 has no hardware float */

void multiply( int ar, int aj, int br, int bj, int* cr, int* cj )
{
	*cr = ar * br - aj * bj;
	*cj = aj * br + ar * bj;
}

int polar_discriminant( int ar, int aj, int br, int bj )
{
	int cr, cj;
	double angle;
	multiply( ar, aj, br, -bj, &cr, &cj );
	angle = atan2( (double)cj, (double)cr );
	return (int)( angle / 3.14159 * ( 1 << 14 ) );
}

int fast_atan2( int y, int x )
/* pre scaled for int16 */
{
	int yabs, angle;
	int pi4 = ( 1 << 12 ), pi34 = 3 * ( 1 << 12 ); // note pi = 1<<14
	if( x == 0 && y == 0 ) {
		return 0;
	}
	yabs = y;
	if( yabs < 0 ) {
		yabs = -yabs;
	}
	if( x >= 0 ) {
		angle = pi4 - pi4 * ( x - yabs ) / ( x + yabs );
	}
	else {
		angle = pi34 - pi4 * ( x + yabs ) / ( yabs - x );
	}
	if( y < 0 ) {
		return -angle;
	}
	return angle;
}

int polar_disc_fast( int ar, int aj, int br, int bj )
{
	int cr, cj;
	multiply( ar, aj, br, -bj, &cr, &cj );
	return fast_atan2( cj, cr );
}

int atan_lut_init( void )
{
	int i = 0;

	atan_lut = malloc( atan_lut_size * sizeof( int ) );

	for( i = 0; i < atan_lut_size; i++ ) {
		atan_lut[i] = (int)( atan( (double)i / ( 1 << atan_lut_coef ) ) / 3.14159 * ( 1 << 14 ) );
	}

	return 0;
}

int polar_disc_lut( int ar, int aj, int br, int bj )
{
	int cr, cj, x, x_abs;

	multiply( ar, aj, br, -bj, &cr, &cj );

	/* special cases */
	if( cr == 0 || cj == 0 ) {
		if( cr == 0 && cj == 0 ) {
			return 0;
		}
		if( cr == 0 && cj > 0 ) {
			return 1 << 13;
		}
		if( cr == 0 && cj < 0 ) {
			return -( 1 << 13 );
		}
		if( cj == 0 && cr > 0 ) {
			return 0;
		}
		if( cj == 0 && cr < 0 ) {
			return 1 << 14;
		}
	}

	/* real range -32768 - 32768 use 64x range -> absolute maximum: 2097152 */
	x = ( cj << atan_lut_coef ) / cr;
	x_abs = abs( x );

	if( x_abs >= atan_lut_size ) {
		/* we can use linear range, but it is not necessary */
		return ( cj > 0 ) ? 1 << 13 : -( 1 << 13 );
	}

	if( x > 0 ) {
		return ( cj > 0 ) ? atan_lut[x] : atan_lut[x] - ( 1 << 14 );
	}
	else {
		return ( cj > 0 ) ? ( 1 << 14 ) - atan_lut[-x] : -atan_lut[-x];
	}

	return 0;
}

void fm_demod( struct demod_state* fm )
{
	int i, pcm;
	int16_t* lp = fm->lowpassed;
	pcm = polar_discriminant( lp[0], lp[1], fm->pre_r, fm->pre_j );
	fm->result[0] = (int16_t)pcm;
	for( i = 2; i < ( fm->lp_len - 1 ); i += 2 ) {
		switch( fm->custom_atan ) {
		case 0:
			pcm = polar_discriminant( lp[i], lp[i + 1], lp[i - 2], lp[i - 1] );
			break;
		case 1:
			pcm = polar_disc_fast( lp[i], lp[i + 1], lp[i - 2], lp[i - 1] );
			break;
		case 2:
			pcm = polar_disc_lut( lp[i], lp[i + 1], lp[i - 2], lp[i - 1] );
			break;
		}
		fm->result[i / 2] = (int16_t)pcm;
	}
	fm->pre_r = lp[fm->lp_len - 2];
	fm->pre_j = lp[fm->lp_len - 1];
	fm->result_len = fm->lp_len / 2;
}

void am_demod( struct demod_state* fm )
// todo, fix this extreme laziness
{
	int i, pcm;
	int16_t* lp = fm->lowpassed;
	int16_t* r = fm->result;
	for( i = 0; i < fm->lp_len; i += 2 ) {
		// hypot uses floats but won't overflow
		// r[i/2] = (int16_t)hypot(lp[i], lp[i+1]);
		pcm = lp[i] * lp[i];
		pcm += lp[i + 1] * lp[i + 1];
		r[i / 2] = (int16_t)sqrt( pcm ) * fm->output_scale;
	}
	fm->result_len = fm->lp_len / 2;
	// lowpass? (3khz)  highpass?  (dc)
}

void usb_demod( struct demod_state* fm )
{
	int i, pcm;
	int16_t* lp = fm->lowpassed;
	int16_t* r = fm->result;
	for( i = 0; i < fm->lp_len; i += 2 ) {
		pcm = lp[i] + lp[i + 1];
		r[i / 2] = (int16_t)pcm * fm->output_scale;
	}
	fm->result_len = fm->lp_len / 2;
}

void lsb_demod( struct demod_state* fm )
{
	int i, pcm;
	int16_t* lp = fm->lowpassed;
	int16_t* r = fm->result;
	for( i = 0; i < fm->lp_len; i += 2 ) {
		pcm = lp[i] - lp[i + 1];
		r[i / 2] = (int16_t)pcm * fm->output_scale;
	}
	fm->result_len = fm->lp_len / 2;
}

void raw_demod( struct demod_state* fm )
{
	int i;
	for( i = 0; i < fm->lp_len; i++ ) {
		fm->result[i] = (int16_t)fm->lowpassed[i];
	}
	fm->result_len = fm->lp_len;
}

void deemph_filter( struct demod_state* fm )
{
	static int avg; // cheating...
	int i, d;
	// de-emph IIR
	// avg = avg * (1 - alpha) + sample * alpha;
	for( i = 0; i < fm->result_len; i++ ) {
		d = fm->result[i] - avg;
		if( d > 0 ) {
			avg += ( d + fm->deemph_a / 2 ) / fm->deemph_a;
		}
		else {
			avg += ( d - fm->deemph_a / 2 ) / fm->deemph_a;
		}
		fm->result[i] = (int16_t)avg;
	}
}

void dc_block_filter( struct demod_state* fm )
{
	int i, avg;
	int64_t sum = 0;
	for( i = 0; i < fm->result_len; i++ ) {
		sum += fm->result[i];
	}
	avg = sum / fm->result_len;
	avg = ( avg + fm->dc_avg * 9 ) / 10;
	for( i = 0; i < fm->result_len; i++ ) {
		fm->result[i] -= avg;
	}
	fm->dc_avg = avg;
}

int mad( int16_t* samples, int len, int step )
/* mean average deviation */
{
	int i = 0, sum = 0, ave = 0;
	if( len == 0 ) {
		return 0;
	}
	for( i = 0; i < len; i += step ) {
		sum += samples[i];
	}
	ave = sum / ( len * step );
	sum = 0;
	for( i = 0; i < len; i += step ) {
		sum += abs( samples[i] - ave );
	}
	return sum / ( len / step );
}

int rms( int16_t* samples, int len, int step )
/* largely lifted from rtl_power */
{
	int i;
	long p, t, s;
	double dc, err;

	p = t = 0L;
	for( i = 0; i < len; i += step ) {
		s = (long)samples[i];
		t += s;
		p += s * s;
	}
	/* correct for dc offset in squares */
	dc = (double)( t * step ) / (double)len;
	err = t * 2 * dc - dc * dc * len;

	return (int)sqrt( ( p - err ) / len );
}

void arbitrary_upsample( int16_t* buf1, int16_t* buf2, int len1, int len2 )
/* linear interpolation, len1 < len2 */
{
	int i = 1;
	int j = 0;
	int tick = 0;
	double frac; // use integers...
	while( j < len2 ) {
		frac = (double)tick / (double)len2;
		buf2[j] = ( int16_t )( buf1[i - 1] * ( 1 - frac ) + buf1[i] * frac );
		j++;
		tick += len1;
		if( tick > len2 ) {
			tick -= len2;
			i++;
		}
		if( i >= len1 ) {
			i = len1 - 1;
			tick = len2;
		}
	}
}

void arbitrary_downsample( int16_t* buf1, int16_t* buf2, int len1, int len2 )
/* fractional boxcar lowpass, len1 > len2 */
{
	int i = 1;
	int j = 0;
	int tick = 0;
	double remainder = 0;
	double frac; // use integers...
	buf2[0] = 0;
	while( j < len2 ) {
		frac = 1.0;
		if( ( tick + len2 ) > len1 ) {
			frac = (double)( len1 - tick ) / (double)len2;
		}
		buf2[j] += ( int16_t )( (double)buf1[i] * frac + remainder );
		remainder = (double)buf1[i] * ( 1.0 - frac );
		tick += len2;
		i++;
		if( tick > len1 ) {
			j++;
			buf2[j] = 0;
			tick -= len1;
		}
		if( i >= len1 ) {
			i = len1 - 1;
			tick = len1;
		}
	}
	for( j = 0; j < len2; j++ ) {
		buf2[j] = buf2[j] * len2 / len1;
	}
}

void arbitrary_resample( int16_t* buf1, int16_t* buf2, int len1, int len2 )
/* up to you to calculate lengths and make sure it does not go OOB
 * okay for buffers to overlap, if you are downsampling */
{
	if( len1 < len2 ) {
		arbitrary_upsample( buf1, buf2, len1, len2 );
	}
	else {
		arbitrary_downsample( buf1, buf2, len1, len2 );
	}
}

void full_demod( struct demod_state* d )
{
	int i;
	int sr = 0;
	low_pass( d );

	sr = rms( d->lowpassed, d->lp_len, 1 );
	if( sr > 0 ) {
		d->signal = sr;
	}

	if( sr < d->squelch_level ) {
		//printf("muting due to %d < %d\n", sr, d->squelch_level);
		for( i = 0; i < d->lp_len; i++ ) {
			d->lowpassed[i] = 0;
		}
	}

	d->mode_demod( d ); /* lowpassed -> result */

	// if( d->mode_demod == &raw_demod ) {
	//	return;
	//}
	/* todo, fm noise squelch */
	// use nicer filter here too?
	// if (d->post_downsample > 1) {
	//	d->result_len = low_pass_simple(d->result, d->result_len, d->post_downsample);}
	// if (d->deemph) {
	//	deemph_filter(d);}
	// if (d->dc_block) {
	//	dc_block_filter(d);}
	// if (d->rate_out2 > 0) {
	//	low_pass_real(d);
	//	//arbitrary_resample(d->result, d->result, d->result_len, d->result_len * d->rate_out2 /
	// d->rate_out);
	//}
}

static int rtlsdr_callback( unsigned char* buf, uint32_t len, void* ctx )
{
	int i;
	struct dongle_state* s = ctx;
	struct demod_state* d = s->demod_target;
	int signal;

	if( !s->offset_tuning ) {
		rotate_90( buf, len );
	}
	for( i = 0; i < (int)len; i++ ) {
		s->buf16[i] = (int16_t)buf[i] - 127;
	}

	while( 1 ) {
		pthread_rwlock_wrlock( &d->rw );
		if( d->signal == -1 ) {
			pthread_rwlock_unlock( &d->rw );
			safe_cond_signal( &d->ready, &d->ready_m );
			usleep( 100 );
			continue;
		}
		memcpy( d->lowpassed, s->buf16, 2 * len );
		d->lp_len = len;
		signal = d->signal;
		d->signal = -1;
		pthread_rwlock_unlock( &d->rw );
		break;
	}
	safe_cond_signal( &d->ready, &d->ready_m );
	return signal;
}

static void optimal_settings( int freq, int rate );

#define BUF_SIZE 1024

// wait this many iterations before checking signal strenght
// this is to reduce chirps which happen between freq changes
// we must always wait at least one cycle due to the way we check
// the last signal, and not current signal
#define BLACKOUT_PERIOD 5
#define SAMPLE_PERIOD 5

static void* dongle_thread_fn( void* arg )
{
	struct dongle_state* s = arg;
	unsigned char buf[BUF_SIZE];
	int len;

	int freq_i = -1;
	int freq = 0;

	int blackout = BLACKOUT_PERIOD;
	int sample_wait = BLACKOUT_PERIOD;
	int squelch_wait = 0;
	int skip_wait = 0;
	int last_signal = -1;
	bool next = true;

	bool can_print = true;
	bool scanning = true;
	bool signal_found = false;

	char *freq_desc = "";

	int scan_squelch = 0;
	int open_squelch = 0;
	int open_duration;

	int ii = 0;

	while( !do_exit ) {

		// change freq
		if( next ) {
			if( scanning ) {
				if( ++freq_i >= controller.freq_len ) {
					freq_i = 0;
				}

				blackout = BLACKOUT_PERIOD;
				sample_wait = SAMPLE_PERIOD;
				skip_wait = controller.freqs[freq_i].skip_duration;
				squelch_wait = 0;

				freq = controller.freqs[freq_i].freq;
				freq_desc = controller.freqs[freq_i].desc;
				scan_squelch = controller.freqs[freq_i].scan_squelch;
				open_duration = controller.freqs[freq_i].open_duration;
				open_squelch = controller.freqs[freq_i].open_squelch;
				
				// set to huge value to silence it during blackout.
				s->demod_target->squelch_level = 10000;
			} else {
				blackout = 0;
				sample_wait = 0;
				skip_wait = -1;
				squelch_wait = -1;
				freq_desc = "manual";

				open_duration = -1;
				open_squelch = 0;
				s->demod_target->squelch_level = open_squelch;
			}

			next = 0;
			can_print = true;

			// TODO lock

			assert( freq > 100000000 );
			optimal_settings( freq, s->demod_target->rate_in );
			rtlsdr_set_center_freq( s->dev, s->freq );
			verbose_reset_buffer( s->dev );
			// printf("set audio squelch to %d\n", s->demod_target->squelch_level);
			signal_found = false;
		}

		if( can_print || ( ii++ % 300 ) == 0 ) {
			int percentage = freq_i * 100 / controller.freq_len;
			clear();
			int mhz = freq / 1000000;
			int khz = (freq / 1000) % 1000;
			mvprintw( 0,
					  0,
					  "%03d.%03d MHz% 20s [% 30s]; signal: %04d; scan: %02d%%%s\n",
					  mhz, khz,
					  signal_found ? " [squelch-open]" : "",
					  freq_desc,
					  last_signal,
					  percentage,
					  scanning ? " [scanning]" : " [manual]" );
			mvprintw( 4, 0, "keys: j/k +/- 5kHz; [s]kip; spacebar: scanner pause; [q]uit" );
			refresh();
			can_print = false;
		}


		int r = rtlsdr_read_sync( s->dev, buf, BUF_SIZE, &len );
		if( r < 0 ) {
			printf( "failed to read: %d\n", r );
			return 0;
		}

		//printf("feed data %d\n", len);
		last_signal = rtlsdr_callback( buf, len, s );
		if( last_signal == -1 ) {
			printf( "dropped data %d\n", len );
			continue;
		}

		pthread_mutex_lock( &controller.hop_m );

		if( controller.freq_adjust ) {
			freq += controller.freq_adjust;
			controller.freq_adjust = 0;
			next = true;
			assert( controller.scanning == false );
			scanning = false;
			can_print = true;
			pthread_mutex_unlock( &controller.hop_m );
			continue;
		}

		if( controller.skip ) {
			next = true;
			controller.skip = false;
			pthread_mutex_unlock( &controller.hop_m );
			continue;
		}

		if( scanning != controller.scanning ) {
			scanning = controller.scanning;
			can_print = true;
			if( scanning ) {
				pthread_mutex_unlock( &controller.hop_m );
				next = true;
				continue;
			}
		}
		if( !controller.scanning ) {
			pthread_mutex_unlock( &controller.hop_m );
			continue;
		}
		pthread_mutex_unlock( &controller.hop_m );

		if( blackout > 0 ) {
			blackout--;
			continue;
		}
		if( last_signal > scan_squelch ) {
			// reset the timer
			squelch_wait = open_duration;

			// TODO lock
			s->demod_target->squelch_level = open_squelch;

			if( !signal_found ) {
				// dont refresh too quickly
				can_print = true;
			}

			// since we heard something, leave the channel open for longer
			squelch_wait = skip_wait;
			signal_found = true;
			continue;
		}

		if( sample_wait > 0 ) {
			sample_wait--;
			continue;
		}

		if( skip_wait == 0 ) {
			// printf("next due to skip_wait\n");
			next = true;
			continue;
		}
		if( skip_wait > 0 ) {
			skip_wait--;
		}

		if( squelch_wait == 0 ) {
			// printf("next due to squelch_wait\n");
			next = true;
			continue;
		}
		if( squelch_wait > 0 ) {
			squelch_wait--;
		}
	}

	return 0;
}

static void* demod_thread_fn( void* arg )
{
	struct demod_state* d = arg;
	struct output_state* o = d->output_target;
	while( !do_exit ) {
		safe_cond_wait( &d->ready, &d->ready_m );
		pthread_rwlock_wrlock( &d->rw );
		d->signal = 0;
		// printf("process data\n");
		full_demod( d );
		pthread_rwlock_unlock( &d->rw );
		if( d->exit_flag ) {
			do_exit = 1;
		}

		pthread_rwlock_wrlock( &o->rw );
		memcpy( o->result, d->result, 2 * d->result_len );
		o->result_len = d->result_len;
		pthread_rwlock_unlock( &o->rw );
		safe_cond_signal( &o->ready, &o->ready_m );
	}
	return 0;
}

static void* output_thread_fn( void* arg )
{
	int error;
	int res;
	struct output_state* s = arg;
	while( !do_exit ) {
		// use timedwait and pad out under runs
		safe_cond_wait( &s->ready, &s->ready_m );
		pthread_rwlock_rdlock( &s->rw );

		// printf("write audio\n");
		res = pa_simple_write( pa_handle, s->result, 2 * s->result_len, &error );
		if( res != 0 ) {
			printf( "failed to write audio\n" );
		}

		pthread_rwlock_unlock( &s->rw );
	}
	return 0;
}

static void* controller_thread_fn( void* arg )
{
	int key;
	while( !do_exit ) {
		key = getch();
		pthread_mutex_lock( &controller.hop_m );
		switch( key ) {
		case 32: // spacebar
			controller.scanning = !controller.scanning;
			break;
		case 115: // s
			controller.skip = true;
			controller.scanning = true;
			break;
		case 107: // up
			controller.scanning = false;
			controller.freq_adjust = 5000;
			break;
		case 106: // down
			controller.scanning = false;
			controller.freq_adjust = -5000;
			break;
		case 113: // q
			do_exit = true;
			break;
		default:
			printf( "unsupported key: %d\n", key );
		}
		pthread_mutex_unlock( &controller.hop_m );
	}
	return NULL;
}

static void optimal_settings( int freq, int rate )
{
	// giant ball of hacks
	// seems unable to do a single pass, 2:1
	int capture_freq, capture_rate;
	struct dongle_state* d = &dongle;
	struct demod_state* dm = &demod;
	dm->downsample = ( 1000000 / dm->rate_in ) + 1;
	if( dm->downsample_passes ) {
		dm->downsample_passes = (int)log2( dm->downsample ) + 1;
		dm->downsample = 1 << dm->downsample_passes;
	}
	capture_freq = freq;
	capture_rate = dm->downsample * dm->rate_in;
	if( !d->offset_tuning ) {
		capture_freq = freq + capture_rate / 4;
	}
	dm->output_scale = ( 1 << 15 ) / ( 128 * dm->downsample );
	if( dm->output_scale < 1 ) {
		dm->output_scale = 1;
	}
	if( dm->mode_demod == &fm_demod ) {
		dm->output_scale = 1;
	}
	d->freq = (uint32_t)capture_freq;
	d->rate = (uint32_t)capture_rate;
}

void dongle_init( struct dongle_state* s )
{
	s->rate = DEFAULT_SAMPLE_RATE;
	s->gain = AUTO_GAIN; // tenths of a dB
	s->mute = 0;
	s->direct_sampling = 0;
	s->offset_tuning = 0;
	s->demod_target = &demod;
}

void demod_init( struct demod_state* s )
{
	s->rate_in = DEFAULT_SAMPLE_RATE;
	s->rate_out = DEFAULT_SAMPLE_RATE;
	s->squelch_level = 0;
	s->downsample_passes = 0;
	s->comp_fir_size = 0;
	s->prev_index = 0;
	s->post_downsample = 1; // once this works, default = 4
	s->custom_atan = 0;
	s->deemph = 0;
	s->rate_out2 = -1; // flag for disabled
	s->mode_demod = &fm_demod;
	s->pre_j = s->pre_r = s->now_r = s->now_j = 0;
	s->prev_lpr_index = 0;
	s->deemph_a = 0;
	s->now_lpr = 0;
	s->dc_block = 0;
	s->dc_avg = 0;
	pthread_rwlock_init( &s->rw, NULL );
	pthread_cond_init( &s->ready, NULL );
	pthread_mutex_init( &s->ready_m, NULL );
	s->output_target = &output;
	s->signal = 0;
}

void demod_cleanup( struct demod_state* s )
{
	pthread_rwlock_destroy( &s->rw );
	pthread_cond_destroy( &s->ready );
	pthread_mutex_destroy( &s->ready_m );
}

void output_init( struct output_state* s )
{
	s->rate = DEFAULT_SAMPLE_RATE;
	pthread_rwlock_init( &s->rw, NULL );
	pthread_cond_init( &s->ready, NULL );
	pthread_mutex_init( &s->ready_m, NULL );
}

void output_cleanup( struct output_state* s )
{
	pthread_rwlock_destroy( &s->rw );
	pthread_cond_destroy( &s->ready );
	pthread_mutex_destroy( &s->ready_m );
}

void controller_init( struct controller_state* s )
{
	s->freq_len = 0;
	s->wb_mode = 0;
	s->scanning = true;
	s->skip = false;
	pthread_cond_init( &s->hop, NULL );
	pthread_mutex_init( &s->hop_m, NULL );
}

void controller_cleanup( struct controller_state* s )
{
	pthread_cond_destroy( &s->hop );
	pthread_mutex_destroy( &s->hop_m );
}

const char* chomp( char* s )
{
	while( s[0] == ' ' || s[0] == '\t' || s[0] == '\n' )
		s++;
	return s;
}

int parse_freqs( const char* path, struct controller_state* controller )
{
	FILE* fp = fopen( path, "rb" );
	if( !fp ) {
		printf( "failed to open %s\n", path );
		return 1;
	}

	char line[1024];
	char desc[1024];

	int freq, squelch, open_squelch, open_duration, skip_duration;

	for( int i = 0; i < FREQUENCIES_LIMIT; ) {

		if( fgets( line, 1024, fp ) == NULL ) {
			return 0;
		}

		const char* l = chomp( line );

		desc[0] = '\0';
		int res = sscanf( l,
						  "%d %d %d %d %d %s",
						  &freq,
						  &squelch,
						  &open_squelch,
						  &open_duration,
						  &skip_duration,
						  desc );
		if( res >= 5 ) {
			controller->freqs[i].freq = freq;
			controller->freqs[i].scan_squelch = squelch;
			controller->freqs[i].open_squelch = open_squelch;
			controller->freqs[i].open_duration = open_duration;
			controller->freqs[i].skip_duration = skip_duration;
			controller->freqs[i].desc = strdup( desc[0] ? desc : "unknown" );
			controller->freq_len = i + 1;
			i++;
		}
		else {
			if( l[0] != '#' && l[0] != '\0' ) {
				printf( "failed to read line \"%s\"\n", l );
				assert( 0 );
			}
		}
	}
	assert( 0 );
}

int main( int argc, char** argv )
{
	struct sigaction sigact;
	int r;
	int dev_given = 0;
	int enable_biastee = 0;
	dongle_init( &dongle );
	demod_init( &demod );
	output_init( &output );
	controller_init( &controller );

	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE, .rate = DEFAULT_SAMPLE_RATE, .channels = 1};

	int error;
	pa_handle = pa_simple_new(
		NULL, "alexplayer", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error );
	if( pa_handle == NULL ) {
		fprintf( stderr, __FILE__ ": pa_simple_new() failed: %s\n", pa_strerror( error ) );
		assert( 0 );
	}

	if( optind >= argc ) {
		printf( "missing frequencies file\n" );
		return 1;
	}

	const char* path = argv[optind];
	if( parse_freqs( path, &controller ) ) {
		return 1;
	}
	for( int i = 0; i < controller.freq_len; i++ ) {
		printf( "%d\n", controller.freqs[i].freq );
	}
	if( controller.freq_len == 0 ) {
		printf( "no frequencies loaded from %s\n", path );
		return 1;
	}

	// init ncurses
	initscr();
	clear();
	noecho();
	cbreak();

	/* quadruple sample_rate to limit to Δθ to ±π/2 */
	demod.rate_in *= demod.post_downsample;

	if( !output.rate ) {
		output.rate = demod.rate_out;
	}

	ACTUAL_BUF_LENGTH = lcm_post[demod.post_downsample] * DEFAULT_BUF_LENGTH;

	if( !dev_given ) {
		dongle.dev_index = verbose_device_search( "0" );
	}

	if( dongle.dev_index < 0 ) {
		exit( 1 );
	}

	r = rtlsdr_open( &dongle.dev, (uint32_t)dongle.dev_index );
	if( r < 0 ) {
		fprintf( stderr, "Failed to open rtlsdr device #%d.\n", dongle.dev_index );
		exit( 1 );
	}
	sigact.sa_handler = sighandler;
	sigemptyset( &sigact.sa_mask );
	sigact.sa_flags = 0;
	sigaction( SIGINT, &sigact, NULL );
	sigaction( SIGTERM, &sigact, NULL );
	sigaction( SIGQUIT, &sigact, NULL );
	sigaction( SIGPIPE, &sigact, NULL );

	if( demod.deemph ) {
		demod.deemph_a = (int)round( 1.0 / ( ( 1.0 - exp( -1.0 / ( demod.rate_out * 75e-6 ) ) ) ) );
	}

	/* Set the tuner gain */
	if( dongle.gain == AUTO_GAIN ) {
		verbose_auto_gain( dongle.dev );
	}
	else {
		dongle.gain = nearest_gain( dongle.dev, dongle.gain );
		verbose_gain_set( dongle.dev, dongle.gain );
	}

	rtlsdr_set_bias_tee( dongle.dev, enable_biastee );
	if( enable_biastee )
		fprintf( stderr, "activated bias-T on GPIO PIN 0\n" );

	verbose_ppm_set( dongle.dev, dongle.ppm_error );

	/* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer( dongle.dev );

	optimal_settings( controller.freqs[0].freq, demod.rate_in );
	verbose_set_frequency( dongle.dev, dongle.freq );
	verbose_set_sample_rate( dongle.dev, dongle.rate );

	usleep( 1000000 );

	pthread_create( &controller.thread, NULL, controller_thread_fn, (void*)( &output ) );
	pthread_create( &output.thread, NULL, output_thread_fn, (void*)( &output ) );
	pthread_create( &demod.thread, NULL, demod_thread_fn, (void*)( &demod ) );
	pthread_create( &dongle.thread, NULL, dongle_thread_fn, (void*)( &dongle ) );

	while( !do_exit ) {
		usleep( 100000 );
	}

	if( do_exit ) {
		fprintf( stderr, "\nUser cancel, exiting...\n" );
	}
	else {
		fprintf( stderr, "\nLibrary error %d, exiting...\n", r );
	}

	rtlsdr_cancel_async( dongle.dev );
	pthread_join( dongle.thread, NULL );
	safe_cond_signal( &demod.ready, &demod.ready_m );
	pthread_join( demod.thread, NULL );
	safe_cond_signal( &output.ready, &output.ready_m );
	pthread_join( output.thread, NULL );
	safe_cond_signal( &controller.hop, &controller.hop_m );
	pthread_join( controller.thread, NULL );

	// dongle_cleanup(&dongle);
	demod_cleanup( &demod );
	output_cleanup( &output );
	controller_cleanup( &controller );

	rtlsdr_close( dongle.dev );

	endwin();
	return 0;
}

// vim: tabstop=8:softtabstop=8:shiftwidth=8:noexpandtab

///*
// * rtl_fm_scanner - a command-line based scanner for rtl sdr.
// * fork from rtl-sdr's  receiver.
// * This code is forked from rtl_fm.c from the rtl-sdr project which is GPL.
// * Modified by Alex Couture-Beil <rtl-scanner@mofo.ca>
// *
// **************************************************************************
// * Original rtl-sdr copyright:
// **************************************************************************
// *
// * rtl-sdr, turns your Realtek RTL2832 based DVB dongle into a SDR receiver
// * Copyright (C) 2012 by Steve Markgraf <steve@steve-m.de>
// * Copyright (C) 2012 by Hoernchen <la@tfc-server.de>
// * Copyright (C) 2012 by Kyle Keen <keenerd@gmail.com>
// * Copyright (C) 2013 by Elias Oenal <EliasOenal@gmail.com>
// *
// * This program is free software: you can redistribute it and/or modify
// * it under the terms of the GNU General Public License as published by
// * the Free Software Foundation, either version 2 of the License, or
// * (at your option) any later version.
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// * GNU General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License
// * along with this program.  If not, see <http://www.gnu.org/licenses/>.
// */
//

#include "rtl_fm_scanner.h"

#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include <unistd.h>

#include <math.h>
#include <pthread.h>
#include <pulse/error.h>
#include <pulse/simple.h>

#include "rtl-sdr.h"

#define IDLE_MODE 0
#define SAMPLE_MODE 1
#define LISTEM_MODE 2

#define DEFAULT_SAMPLE_RATE 24000
#define DEFAULT_BUF_LENGTH ( 1 * 16384 )
#define MAXIMUM_OVERSAMPLE 16
#define MAXIMUM_BUF_LENGTH ( MAXIMUM_OVERSAMPLE * DEFAULT_BUF_LENGTH )

//const int rate = 1000000;
const int rate = 2800000;
const int bin_e = 10;
const int bin_len = 1 << bin_e; // 1024
const int buf_len = 16384*8;

// FIXME globals
static int* atan_lut = NULL;
static int atan_lut_size = 131072; /* 512 KB */
static int atan_lut_coef = 8;
pa_simple* pa_handle;

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
	//struct output_state* output_target;

	int signal; // not processed: -1;
};

struct radio_scanner {
	dbms_cb_t dbms_cb;
	void *dbms_cb_user_data;
	rtlsdr_dev_t *dev;
	pthread_mutex_t mutex;
	int freq_low, freq_high;
	int mode;

	struct demod_state demod_state;
};
//int step = (double)rate / (double)(bin_len);
//double *dbms = NULL; //[16384];
//int num_scans = 0;

int sigs_low_freq;
int sigs_num_steps;
int sigs_step;
double *sigs;

double clamp_min_dbm = -100.0;
double clamp_max_dbm = 0.0;

//int freq_low = 89000000;
//int freq_high = 107000000;

//int freq_low = 144000000;
//int freq_high = 148000000;

//int freq_low = 440000000;
//int freq_high = 460000000;


// vhf range
//int freq_low = 130000000;
//int freq_high = 175000000;



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

#define BUFFER_DUMP			(1<<12)


int16_t* Sinewave;
double* power_table;
int N_WAVE, LOG2_N_WAVE;
int next_power;

/* {length, coef, coef, coef}  and scaled by 2^15
   for now, only length 9, optimal way to get +85% bandwidth */
#define CIC_TABLE_MAX 10
int cic_9_tables[][10] = {
	{0,},
	{9, -156,  -97, 2798, -15489, 61019, -15489, 2798,  -97, -156},
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

/* FFT based on fix_fft.c by Roberts, Slaney and Bouras
   http://www.jjj.de/fft/fftpage.html
   16 bit ints for everything
   -32768..+32768 maps to -1.0..+1.0
*/

void sine_table(int size)
{
	int i;
	double d;
	LOG2_N_WAVE = size;
	N_WAVE = 1 << LOG2_N_WAVE;
	Sinewave = malloc(sizeof(int16_t) * N_WAVE*3/4);
	power_table = malloc(sizeof(double) * N_WAVE);
	for (i=0; i<N_WAVE*3/4; i++)
	{
		d = (double)i * 2.0 * M_PI / N_WAVE;
		Sinewave[i] = (int)round(32767*sin(d));
		//printf("%i\n", Sinewave[i]);
	}
}

static inline int16_t FIX_MPY(int16_t a, int16_t b)
/* fixed point multiply and scale */
{
	int c = ((int)a * (int)b) >> 14;
	b = c & 0x01;
	return (c >> 1) + b;
}

int fix_fft(int16_t iq[], int m)
/* interleaved iq[], 0 <= n < 2**m, changes in place */
{
	int mr, nn, i, j, l, k, istep, n, shift;
	int16_t qr, qi, tr, ti, wr, wi;
	n = 1 << m;
	if (n > N_WAVE)
		{return -1;}
	mr = 0;
	nn = n - 1;
	/* decimation in time - re-order data */
	for (m=1; m<=nn; ++m) {
		l = n;
		do
			{l >>= 1;}
		while (mr+l > nn);
		mr = (mr & (l-1)) + l;
		if (mr <= m)
			{continue;}
		// real = 2*m, imag = 2*m+1
		tr = iq[2*m];
		iq[2*m] = iq[2*mr];
		iq[2*mr] = tr;
		ti = iq[2*m+1];
		iq[2*m+1] = iq[2*mr+1];
		iq[2*mr+1] = ti;
	}
	l = 1;
	k = LOG2_N_WAVE-1;
	while (l < n) {
		shift = 1;
		istep = l << 1;
		for (m=0; m<l; ++m) {
			j = m << k;
			wr =  Sinewave[j+N_WAVE/4];
			wi = -Sinewave[j];
			if (shift) {
				wr >>= 1; wi >>= 1;}
			for (i=m; i<n; i+=istep) {
				j = i + l;
				tr = FIX_MPY(wr,iq[2*j]) - FIX_MPY(wi,iq[2*j+1]);
				ti = FIX_MPY(wr,iq[2*j+1]) + FIX_MPY(wi,iq[2*j]);
				qr = iq[2*i];
				qi = iq[2*i+1];
				if (shift) {
					qr >>= 1; qi >>= 1;}
				iq[2*j] = qr - tr;
				iq[2*j+1] = qi - ti;
				iq[2*i] = qr + tr;
				iq[2*i+1] = qi + ti;
			}
		}
		--k;
		l = istep;
	}
	return 0;
}

void retune(rtlsdr_dev_t *d, int freq)
{
	uint8_t dump[BUFFER_DUMP];
	int n_read;
	//printf("tuning to %d\n", freq);
	rtlsdr_set_center_freq(d, (uint32_t)freq);
	/* wait for settling and flush buffer */
	usleep(5000);
	rtlsdr_read_sync(d, &dump, BUFFER_DUMP, &n_read);
	if (n_read != BUFFER_DUMP) {
		fprintf(stderr, "Error: bad retune.\n");}
}

void remove_dc(int16_t *data, int length)
/* works on interleaved data */
{
	int i;
	int16_t ave;
	long sum = 0L;
	for (i=0; i < length; i+=2) {
		sum += data[i];
	}
	ave = (int16_t)(sum / (long)(length));
	if (ave == 0) {
		return;}
	for (i=0; i < length; i+=2) {
		data[i] -= ave;
	}
}

long real_conj(int16_t real, int16_t imag)
/* real(n * conj(n)) */
{
	return ((long)real*(long)real + (long)imag*(long)imag);
}

void scanner(struct radio_scanner *rs, int low_freq, double *dbms)
{
	int i, j, n_read, offset;
	int32_t w;
	long tmp;
	double dbm;

	int bw2 = rate / 2;
	int center_freq = low_freq + bw2;

	retune(rs->dev, center_freq);

	uint8_t buf8[buf_len];
	long avg[buf_len];
	memset(avg, 0, buf_len*sizeof(long));
	int samples = 0;

	int16_t fft_buf[buf_len];

	rtlsdr_read_sync(rs->dev, buf8, buf_len, &n_read);
	if (n_read != buf_len) {
		fprintf(stderr, "Error: dropped samples.\n");
		exit(1);
	}

	/* prep for fft */
	for (j=0; j<buf_len; j++) {
		fft_buf[j] = (int16_t)buf8[j] - 127;
	}
	remove_dc(fft_buf, buf_len);
	remove_dc(fft_buf+1, (buf_len) - 1);
	/* window function and fft */
	for (offset=0; offset<(buf_len); offset+=(2*bin_len)) {
		// todo, let rect skip this
		for (j=0; j<bin_len; j++) {
			w =  (int32_t)fft_buf[offset+j*2];
			w *= 256; //(int32_t)(window_coefs[j]);
			//w /= (int32_t)(ds);
			fft_buf[offset+j*2]   = (int16_t)w;
			w =  (int32_t)fft_buf[offset+j*2+1];
			w *= 256; //(int32_t)(window_coefs[j]);
			//w /= (int32_t)(ds);
			fft_buf[offset+j*2+1] = (int16_t)w;
		}
		fix_fft(fft_buf+offset, bin_e);
		for (j=0; j<bin_len; j++) {
			avg[j] += real_conj(fft_buf[offset+j*2], fft_buf[offset+j*2+1]);
		}
		samples += 1;
	}
	/* fix FFT stuff quirks */
	/* nuke DC component (not effective for all windows) */
	avg[0] = avg[1];
	/* FFT is translated by 180 degrees */
	for (i=0; i<bin_len/2; i++) {
		tmp = avg[i];
		avg[i] = avg[i+bin_len/2];
		avg[i+bin_len/2] = tmp;
	}
	

	for (i=0; i<bin_len; i++) {
		dbm  = (double)avg[i];
		dbm /= (double)rate;
		dbm /= (double)samples;
		dbm  = 10 * log10(dbm);
		dbms[i] = dbm;
		//if( isnan(dbm) ) {
		//	printf("avg=%ld rate=%d samples=%d\n", avg[i], rate, samples);
		//}
	}
}

double window(double *dbms, int num_samples)
{
	double sum = 0.0;
	for(int i = 0; i < num_samples; i++ ) {
		sum += dbms[i];
	}
	return sum / num_samples;
}

void find_channels(int low_freq, int step, int num_steps, double *dbms)
{
	double min_dbm = -45.0;

	char buf[1024];

	int win_size = 1;

	printf("---\n");
	int last_max = -1;
	double last_dbm = -99999;
	for (int i=0; i<(num_steps-win_size); i++) {
		double avg = window(dbms+i, win_size);
		if( avg > last_dbm ) {
			last_dbm = avg;
			last_max = i;
		} else {
			if( last_dbm > min_dbm ) {
				int freq = low_freq + (i+win_size/2)*step;
				fmt_freq( buf, freq );
				printf("%s: %lf\n", buf, last_dbm);
			}
			last_dbm = -9999;
		}
	}
}

static void optimal_settings( struct radio_scanner *rs, int freq )
{
	// giant ball of hacks
	// seems unable to do a single pass, 2:1
	int capture_freq, capture_rate;
	//struct dongle_state* d = &dongle;
	struct demod_state* dm = &rs->demod_state;
	dm->downsample = ( 1000000 / dm->rate_in ) + 1;
	if( dm->downsample_passes ) {
		dm->downsample_passes = (int)log2( dm->downsample ) + 1;
		dm->downsample = 1 << dm->downsample_passes;
	}
	capture_freq = freq;
	capture_rate = dm->downsample * dm->rate_in;
	//if( !d->offset_tuning ) {
	//	capture_freq = freq + capture_rate / 4;
	//}
	dm->output_scale = ( 1 << 15 ) / ( 128 * dm->downsample );
	if( dm->output_scale < 1 ) {
		dm->output_scale = 1;
	}
	//if( dm->mode_demod == &fm_demod ) {
	dm->output_scale = 1;
	//}
	//d->freq = (uint32_t)capture_freq;
	//d->rate = (uint32_t)capture_rate;
}

void listen_and_decode(struct radio_scanner *rs)
{
	static int last_freq = 0;

	if( last_freq != rs->freq_low ) {
		last_freq = rs->freq_low;
		optimal_settings(rs, rs->freq_low);
		retune(rs->dev, rs->freq_low);
	}

	int len;
	unsigned char buf[MAXIMUM_BUF_LENGTH];
	uint16_t buf16[MAXIMUM_BUF_LENGTH];
	int r = rtlsdr_read_sync( rs->dev, buf, MAXIMUM_BUF_LENGTH, &len );
	if( r < 0 ) {
		printf( "failed to read: %d\n", r );
		return 0;
	}

	int i;
	//struct dongle_state* s = ctx;
	struct demod_state* d = &rs->demod_state;
	//struct output_state* o = d->output_target;
	int signal;

	//if( !s->offset_tuning ) {
	//	rotate_90( buf, len );
	//}
	for( i = 0; i < (int)len; i++ ) {
		buf16[i] = (int16_t)buf[i] - 127;
	}

	memcpy( d->lowpassed, buf16, 2 * len );
	d->lp_len = len;

	int sr = 0;
	low_pass( d );

	sr = rms( d->lowpassed, d->lp_len, 1 );
	if( sr > 0 ) {
		d->signal = sr;
	}

	//printf("sr: %d\n", sr);
	//if( sr < d->squelch_level ) {
	//	// printf("muting due to %d < %d\n", sr, d->squelch_level);
	//	for( i = 0; i < d->lp_len; i++ ) {
	//		d->lowpassed[i] = 0;
	//	}
	//}

	d->mode_demod( d ); /* lowpassed -> result */

	//pthread_rwlock_wrlock( &o->rw );
	//memcpy( o->result, d->result, 2 * d->result_len );
	//o->result_len = d->result_len;
	//pthread_rwlock_unlock( &o->rw );
	//safe_cond_signal( &o->ready, &o->ready_m );
}


static void* radioscanner( void* arg )
{
	int res;
	int error;
	struct radio_scanner *rs = arg;
	int scan_bw = rs->freq_high - rs->freq_low;
	int num_scans = ( scan_bw / rate ) + 1;
	int step = (double)rate / (double)(bin_len);
	double *dbms = malloc(sizeof(double) * bin_len * num_scans);
	for(;;) {
		pthread_mutex_lock(&(rs->mutex));
		switch( rs->mode ) {
			case SAMPLE_MODE:
				{
					for( int i = 0; i < num_scans; i++ ) {
						int freq = rs->freq_low + i * rate;
						scanner(rs, freq, dbms + i*bin_len);
					}

					rs->dbms_cb(dbms, rs->freq_low, step, bin_len*num_scans, rs->dbms_cb_user_data);
				}
				break;
			case LISTEM_MODE:
			{
				printf("listen %d\n", rs->freq_low);
				// TODO actually tune the radio and decode it
				listen_and_decode(rs);

				// TODO perhaps this needs to be done in a different thread?
				for( int i = 0; i < rs->demod_state.result_len; i++ ) {
					int32_t tmp = rs->demod_state.result[i];
					//tmp *= rs->volume;
					tmp /= 100;
					rs->demod_state.result[i] = tmp;
				}
				if( rs->demod_state.result_len > 0 ) {
					res = pa_simple_write( pa_handle, rs->demod_state.result, 2 * rs->demod_state.result_len, &error );
					if( res != 0 ) {
						printf( "failed to write audio\n" );
					}
				}
			}
				break;
			default:
			break;
		}
		pthread_mutex_unlock(&(rs->mutex));
		usleep(10000);
	}
}

pthread_t thread;
int init_radio(struct radio_scanner **rs, dbms_cb_t dbms_cb, void *user_data)
{
	int r;
	*rs = malloc(sizeof(struct radio_scanner));
	memset(*rs, 0, sizeof(struct radio_scanner));
	(**rs).dbms_cb = dbms_cb;
	(**rs).dbms_cb_user_data = user_data;
	assert(pthread_mutex_init(&(**rs).mutex, NULL) == 0);

	demod_init(&(**rs).demod_state);

	// setup pulse audio
	int error;
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE, .rate = DEFAULT_SAMPLE_RATE, .channels = 1};
	pa_handle = pa_simple_new(
		NULL, "alexscanner", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error );
	if( pa_handle == NULL ) {
		fprintf( stderr, __FILE__ ": pa_simple_new() failed: %s\n", pa_strerror( error ) );
		assert( 0 );
	}


	int dev_index = verbose_device_search("0");
	r = rtlsdr_open(&((**rs).dev), (uint32_t)dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
		exit(1);
	}

	/* Set the tuner gain */
	// can't use auto-gain because it changes between scan tunes
	//verbose_auto_gain(dev);
	int gain = nearest_gain((**rs).dev, 100);
	verbose_gain_set((**rs).dev, gain);

	int ppm_error = 0;
	verbose_ppm_set((**rs).dev, ppm_error);

	int enable_biastee = 0;
	rtlsdr_set_bias_tee((**rs).dev, enable_biastee);
	if (enable_biastee)
		fprintf(stderr, "activated bias-T on GPIO PIN 0\n");

	/* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer((**rs).dev);

	/* actually do stuff */
	rtlsdr_set_sample_rate((**rs).dev, rate);
	sine_table(bin_e);

	pthread_create( &thread, NULL, radioscanner, *rs );

	return 0;
}

int stop_radio(struct radio_scanner *rs)
{
	rtlsdr_close(rs->dev);
}

int radio_sample( struct radio_scanner* rs, int freq_low, int freq_high )
{
	pthread_mutex_lock(&(rs->mutex));
	rs->mode = SAMPLE_MODE;
	rs->freq_low = freq_low;
	rs->freq_high = freq_high;
	pthread_mutex_unlock(&(rs->mutex));
	return 0;
}

int radio_listen( struct radio_scanner* rs, int freq )
{
	pthread_mutex_lock(&(rs->mutex));
	rs->mode = LISTEM_MODE;
	rs->freq_low = freq;
	pthread_mutex_unlock(&(rs->mutex));
	return 0;
}

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
	//s->output_target = &output;
	s->signal = 0;
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


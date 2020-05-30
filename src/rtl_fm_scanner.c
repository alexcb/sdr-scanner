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

#include "rtl-sdr.h"

const int rate = 1000000;
//const int rate = 2800000;
const int bin_e = 10;
const int bin_len = 1 << bin_e;
const int buf_len = 16384;


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


#define BUFFER_DUMP			(1<<12)


static rtlsdr_dev_t *dev = NULL;

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
	printf("turning to %d\n", freq);
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

void scanner(int low_freq, double *dbms)
{
	int i, j, n_read, offset;
	int32_t w;
	long tmp;
	double dbm;

	int bw2 = rate / 2;
	int center_freq = low_freq + bw2;

	retune(dev, center_freq);

	uint8_t buf8[16384];
	long avg[16384];
	memset(avg, 0, 16384*sizeof(long));
	int samples;

	int rate = 1000000;

	int16_t fft_buf[16384];

	rtlsdr_read_sync(dev, buf8, buf_len, &n_read);
	if (n_read != buf_len) {
		fprintf(stderr, "Error: dropped samples.\n");}

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
	}
}

void print_loudest(int low_freq, int step, int num_steps, double *dbms)
{
	printf("start at %d\n", low_freq);

	double max_dbm = -9999.0;
	int max_freq = 0;
	int cur_freq = low_freq;

	for (int i=0; i<num_steps; i++) {
		if( dbms[i] > max_dbm ) {
			max_dbm = dbms[i];
			max_freq = cur_freq;
		}
		cur_freq += step;
	}
	printf("end at %d\n", cur_freq);
	printf("next at %d\n", cur_freq+step);

	if( max_freq ) {
		printf("strongest signal at %d\n", max_freq);
	} else {
		printf("none\n");
	}
}

#include <gdk/gdk.h>
#include <gtk/gtk.h>

GtkWidget *da = NULL;

int hover_freq = -1;

static gboolean mouse_moved(GtkWidget *widget,GdkEvent *event,gpointer user_data)
{
	if (event->type==GDK_MOTION_NOTIFY) {
		GdkEventMotion* e=(GdkEventMotion*)event;
		printf("Coordinates: (%u,%u)\n", (guint)e->x,(guint)e->y);
		hover_freq = e->x;
		gtk_widget_queue_draw( da );
	}
}


int sigs_low_freq;
int sigs_num_steps;
int sigs_step;
double *sigs;

static gboolean
checkerboard_draw (GtkWidget *da,
                   cairo_t   *cr,
                   gpointer   data)
{
	gint i, j, xcount, width, height;

#define CHECK_SIZE 10
#define SPACING 2

	unsigned char buf[32*1024];
	//get_radio_raw_buf( &rs, buf );
	//get_radio_rot_buf( &rs, buf );

	//xcount = 0;
	width = gtk_widget_get_allocated_width (da);
	height = gtk_widget_get_allocated_height (da);
	//i = SPACING;
	int width_per_step = width / sigs_num_steps;
	if( width_per_step <= 0 ) {
		printf("too small\n");
		return TRUE;
	}
	printf("draw %d %d\n", width, width_per_step);
	for( i = 0; i < sigs_num_steps; i++ ) {
		double ss = sigs[i];

		cairo_set_source_rgb( cr, 0, 0, 0 );

		int y = 200 - ss;
		cairo_rectangle( cr, i*width_per_step, y, width_per_step, 1);
		cairo_fill (cr);
	}

	cairo_select_font_face( cr, "Purisa",
			CAIRO_FONT_SLANT_NORMAL,
			CAIRO_FONT_WEIGHT_BOLD);

	cairo_set_font_size(cr, 13);

	int tick_width = 100;
	int num_ticks = width / tick_width;
	if( num_ticks >= 2 ) {
		for(int i = 0; i < num_ticks; i++ ) {
			int x = i * tick_width;
			char s[1024];
			int freq = sigs_low_freq + (x/width_per_step)*sigs_step;
			sprintf(s, "%d", freq);

			cairo_move_to(cr, x, 100);
			cairo_show_text(cr, s);
		}
	}

	if( 0 <= hover_freq && hover_freq < width ) {
		int freq = sigs_low_freq + (hover_freq/width_per_step)*sigs_step;
		char s[1024];
		sprintf(s, "%d", freq);

		cairo_move_to(cr, 50, 150);
		cairo_show_text(cr, s);
	}

	/* return TRUE because we've handled this event, so no
	 * further processing is required.
	 */
	return TRUE;
}

void plot_dbms(int low_freq, int step, int num_steps, double *dbms)
{
	sigs_low_freq = low_freq;
	sigs_num_steps = num_steps;
	sigs_step = step;
	sigs = dbms;
	//int cur_freq = low_freq;
	//for (int i=0; i<num_steps; i++) {
	//	printf("%d %lf\n", cur_freq, dbms[i]);
	//	cur_freq += step;
	//}
	GtkWidget *window = gtk_window_new( GTK_WINDOW_TOPLEVEL );

	GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );


	// drawing area for waterfall
	GtkWidget* frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

	da = gtk_drawing_area_new ();
	/* set a minimum size */
	gtk_widget_set_size_request (da, 100, 100);

	gtk_container_add (GTK_CONTAINER (frame), da);

	g_signal_connect (da, "draw", G_CALLBACK(checkerboard_draw), NULL);
    g_signal_connect (da, "motion-notify-event", G_CALLBACK(mouse_moved), NULL);
	gtk_widget_set_events(da, GDK_POINTER_MOTION_MASK);

	//gtk_window_set_default_size( GTK_WINDOW( window ), 700, 700 );
	gtk_widget_show_all( window );

	GMainLoop* mainloop = NULL;
	mainloop = g_main_loop_new( NULL, FALSE );
	g_main_loop_run( mainloop );

	return 0;
}

int main(int argc, char **argv)
{
	int r;

	int dev_index = verbose_device_search("0");
	r = rtlsdr_open(&dev, (uint32_t)dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
		exit(1);
	}

	/* Set the tuner gain */
	verbose_auto_gain(dev);
	//	gain = nearest_gain(dev, gain);
	//	verbose_gain_set(dev, gain);

	int ppm_error = 0;
	verbose_ppm_set(dev, ppm_error);

	int enable_biastee = 0;
	rtlsdr_set_bias_tee(dev, enable_biastee);
	if (enable_biastee)
		fprintf(stderr, "activated bias-T on GPIO PIN 0\n");

	/* Reset endpoint before we start reading from it (mandatory) */
	verbose_reset_buffer(dev);

	/* actually do stuff */
	rtlsdr_set_sample_rate(dev, rate);
	sine_table(bin_e);


	int step = (double)rate / (double)(bin_len);

	double dbms[16384];

	int freq = 162500000;
	scanner(freq, dbms);
	print_loudest(freq, step, bin_len, dbms);

	//freq = 147000000;
	//scanner(freq, dbms);
	//print_loudest(freq, step, bin_len, dbms);

	gtk_init( &argc, &argv );
	plot_dbms(freq, step, bin_len, dbms);


	rtlsdr_close(dev);
	return r >= 0 ? r : -r;
}

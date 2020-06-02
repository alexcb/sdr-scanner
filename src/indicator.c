
//#include "custom-list.h"
//#include "rtl_fm_scanner.h"
//#include "should_stop.h"
//
#include <assert.h>
//
//#include <libayatana-appindicator/app-indicator.h>
//#include <libdbusmenu-glib/menuitem.h>
//#include <libdbusmenu-glib/server.h>
//
//GMainLoop* mainloop = NULL;
//
//#define ICON_ACTIVE "/home/alex/gh/alexcb/sdr-scanner/radio-active.svg"
//#define ICON_SCANNING "/home/alex/gh/alexcb/sdr-scanner/radio-scanning.svg" // white
//#define ICON_MUTED "/home/alex/gh/alexcb/sdr-scanner/radio-muted.svg"
//#define ICON_BLACK "/home/alex/gh/alexcb/sdr-scanner/radio-black.svg"
//
//GtkWidget* window = NULL;
//GtkWidget* thetreeview = NULL;
//AppIndicator* ci = NULL;
//
//struct radio_scanner* rs = NULL;
//GtkWidget* da = NULL;
//
//static void muted_toggled( GtkCellRendererToggle* cell, gchar* path_str, gpointer data )
//{
//	CustomList* model = (CustomList*)data;
//
//	GtkTreePath* path = gtk_tree_path_new_from_string( path_str );
//	gint selected_track = gtk_tree_path_get_indices( path )[0];
//
//	printf( "toggle mute: %d\n", selected_track );
//
//	CustomRecord* record;
//	record = model->rows[selected_track];
//	record->muted = !record->muted;
//
//	gtk_tree_path_free( path );
//}
//
//static void mute_item_clicked_cb( GtkWidget* widget, gpointer data )
//{
//	struct radio_scanner* rs = (struct radio_scanner*)data;
//	if( gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( widget ) ) ) {
//		printf( "muted\n" );
//		set_radio_volume( rs, 0 );
//		app_indicator_set_icon( ci, ICON_MUTED );
//	}
//	else {
//		printf( "active\n" );
//		set_radio_volume( rs, 100 );
//		app_indicator_set_icon( ci, ICON_SCANNING );
//	}
//}
//
//static void scan_button_toggled( GtkWidget* widget, gpointer data )
//{
//	gboolean toggled = gtk_toggle_button_get_active( widget );
//	if( toggled ) {
//		printf( "scan should be on\n" );
//	} else {
//		printf( "scan should be off\n" );
//	}
//}
//
//static void channels_item_clicked_cb( GtkWidget* widget, gpointer data )
//{
//	printf( "raise it\n" );
//	// gtk_window_maximize( window );
//	gtk_window_present( GTK_WINDOW( window ) );
//}
//
//static void quit_item_clicked_cb( GtkWidget* widget, gpointer data )
//{
//	g_main_loop_quit( mainloop );
//}
//
//// this thread controlls the scanner freq selection + telling the UI which freq is selected
//static void* twidler( void* arg )
//{
//
//	//int freqs[] = {162475000, 162400000};
//
//	//{
//	//	int i = 0;
//	//	for(;;) {
//	//		printf("go %d\n", freqs[i%2]);
//	//		set_radio_freq( &rs, freqs[i%2] );
//	//			usleep( 1000 );
//	//			printf( "signal %d\n", get_radio_signal_strength( &rs ) );
//	//		i++;
//	//		usleep( 1000000 );
//	//	}
//	//}
//
//	
//	CustomList* custom_list = (CustomList*)arg;
//	int i = 0;
//	int j = 0;
//	bool needs_tune = true;
//	GtkTreeIter iter;
//	for( ;; ) {
//		for( ;; ) {
//
//			gtk_widget_queue_draw( da );
//
//			if( needs_tune ) {
//			printf("%d\n", custom_list->rows[j]->frequency);
//			set_radio_freq( &rs, custom_list->rows[j]->frequency );
//			needs_tune = false;
//			}
//
//			custom_list->current_channel = j;
//			gtk_widget_queue_draw( thetreeview );
//			usleep( 10000 );
//			//sleep(1);
//			int sig = get_radio_signal_strength( &rs );
//			if( sig > 200 ) {
//				//printf("found signal\n");
//				continue;
//			}
//			//j = (j+1) % custom_list->num_rows;
//			//needs_tune = true;
//		}
//		// if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( model ), &iter ) ) {
//		//	do {
//		//		gtk_list_store_set(
//		//			model, &iter, COLUMN_ACTIVE_ICON, i == j ? active_freq_pixbuf : NULL, -1 );
//		//		j++;
//		//	} while( gtk_tree_model_iter_next( GTK_TREE_MODEL( model ), &iter ) );
//		//}
//		// i = ( i + 1 ) % 2;
//		usleep( 100000 );
//	}
//}
//
//void on_row_activated( GtkTreeView* tree_view,
//					   GtkTreePath* path,
//					   GtkTreeViewColumn* column,
//					   gpointer user_data )
//{
//	gint selected_track = gtk_tree_path_get_indices( path )[0];
//	printf( "activate %d\n", selected_track );
//}
//
//// typedef struct {
////	GtkApplicationWindow parent_instance;
////
////	int current_width;
////	int current_height;
////	bool is_maximized;
////	bool is_fullscreen;
////} MyApplicationWindow;
////
//// static void my_application_window_store_state (MyApplicationWindow *self)
////{
////	GSettings *settings = g_settings_new ("ca.mofo.sdr-scanner");
////
////	g_settings_set_int (settings, "width", self->current_width);
////	g_settings_set_int (settings, "height", self->current_height);
////	g_settings_set_boolean (settings, "is-maximized", self->is_maximized);
////	g_settings_set_boolean (settings, "is-fullscreen", self->is_fullscreen);
////}
////
//// static void on_window_destroy (GtkWidget *widget)
////{
////  MyApplicationWindow *app_window = MY_APPLICATION_WINDOW (widget);
////
////  // store the state here
////  my_application_window_store_state (app_window);
////
////  // chain up to the parent's implementation
////  GTK_WIDGET_CLASS (my_application_window_parent_class)->destroy (widget);
////}
//
//static gboolean
//checkerboard_draw (GtkWidget *da,
//                   cairo_t   *cr,
//                   gpointer   data)
//{
//	gint i, j, xcount, width, height;
//
//#define CHECK_SIZE 10
//#define SPACING 2
//
//	unsigned char buf[32*1024];
//	//get_radio_raw_buf( &rs, buf );
//	//get_radio_rot_buf( &rs, buf );
//
//	int16_t buf2[32*1024];
//	get_radio_lowpassed( &rs, &buf2 );
//
//
//	printf("draw\n");
//	xcount = 0;
//	width = gtk_widget_get_allocated_width (da);
//	height = gtk_widget_get_allocated_height (da);
//	i = SPACING;
//	for( i = 0; i < width; i++ ) {
//
//		if( i > 1024 ) {
//			printf("done\n");
//			break;
//		}
//		double col = buf[i%1024] / 100.0;
//		col = buf2[i%1024] / 1000.0;
//		if( col > 1.0 )
//			col = 1.0;
//		cairo_set_source_rgb( cr, 0, 0, col );
//
//		cairo_rectangle( cr, i, 0, 1, height);
//		cairo_fill (cr);
//	}
//
//	/* return TRUE because we've handled this event, so no
//	 * further processing is required.
//	 */
//	return TRUE;
//}
//
//int main( int argc, char** argv )
//{
//	GtkWidget* menu = NULL;
//
//	if( argc != 2 ) {
//		fprintf( stderr, "usage: %s <freq.txt>\n", argv[0] );
//		return 1;
//	}
//
//	gtk_init( &argc, &argv );
//
//	//int cbc = 88100000;
//	//int vhf_marine_1 = 162475000;
//	//int vhf_marine_2 = 162400000;
//
//	//int freqs[] = {162475000, 162400000};
//
//	// start radio thread
//	stop_thread_init();
//	int res = init_radio( &rs );
//	if( res ) {
//		fprintf( stderr, "failed to start thread\n" );
//		// return 1;
//	}
//
//	ci = app_indicator_new(
//		"sdr-scanner", ICON_SCANNING, APP_INDICATOR_CATEGORY_APPLICATION_STATUS );
//
//	g_assert( IS_APP_INDICATOR( ci ) );
//	g_assert( G_IS_OBJECT( ci ) );
//
//	app_indicator_set_status( ci, APP_INDICATOR_STATUS_ACTIVE );
//	// app_indicator_set_attention_icon_full( ci, "tray-new-im", "asdfasdf" );
//	// app_indicator_set_title( ci, "Test Inidcator" );
//	// app_indicator_set_icon( ci, "/usr/share/icons/ubuntu-mono-dark/status/24/nm-signal-100.svg"
//	// ); //"/home/alex/gh/alexcb/sdr-scanner/radio.svg" );
//
//	menu = gtk_menu_new();
//
//	// mute
//	GtkWidget* mute_item = gtk_check_menu_item_new_with_label( "mute" );
//	g_signal_connect( mute_item, "activate", G_CALLBACK( mute_item_clicked_cb ), rs );
//	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), mute_item );
//	gtk_widget_show( mute_item );
//
//	// channels
//	GtkWidget* channels_item = gtk_menu_item_new_with_label( "channels" );
//	g_signal_connect( channels_item, "activate", G_CALLBACK( channels_item_clicked_cb ), rs );
//	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), channels_item );
//	gtk_widget_show( channels_item );
//
//	// quit
//	GtkWidget* quit_item = gtk_menu_item_new_with_label( "quit" );
//	g_signal_connect( quit_item, "activate", G_CALLBACK( quit_item_clicked_cb ), NULL );
//	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), quit_item );
//	gtk_widget_show( quit_item );
//
//	app_indicator_set_menu( ci, GTK_MENU( menu ) );
//
//	window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
//	gtk_window_set_icon_from_file( GTK_WINDOW( window ), ICON_SCANNING, NULL );
//
//	GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
//	gtk_container_add( GTK_CONTAINER( window ), vbox );
//
//	//GtkWidget* label = gtk_label_new( "hello scanner" );
//	//gtk_box_pack_start( GTK_BOX( vbox ), label, FALSE, FALSE, 0 );
//
//
//
//
//
//
//
//	// drawing area for waterfall
//	GtkWidget* frame = gtk_frame_new (NULL);
//	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
//	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
//
//	da = gtk_drawing_area_new ();
//	/* set a minimum size */
//	gtk_widget_set_size_request (da, 100, 100);
//
//	gtk_container_add (GTK_CONTAINER (frame), da);
//
//	g_signal_connect (da, "draw", G_CALLBACK(checkerboard_draw), NULL);
//
//
//
//
//
//
//	GtkWidget* scan_button = gtk_toggle_button_new_with_label( "scan" );
//	g_signal_connect( scan_button, "toggled", G_CALLBACK( scan_button_toggled ), rs );
//	gtk_box_pack_start( GTK_BOX( vbox ), scan_button, FALSE, FALSE, 0 );
//
//	GtkWidget* sw = gtk_scrolled_window_new( NULL, NULL );
//	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( sw ), GTK_SHADOW_ETCHED_IN );
//	gtk_scrolled_window_set_policy(
//		GTK_SCROLLED_WINDOW( sw ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
//	gtk_box_pack_start( GTK_BOX( vbox ), sw, TRUE, TRUE, 0 );
//
//	CustomList* customlist;
//	customlist = custom_list_new();
//
//	assert( customlist->active_pixbuf = gdk_pixbuf_new_from_file( ICON_BLACK, NULL ) );
//	assert( customlist->audio_pixbuf = gdk_pixbuf_new_from_file( ICON_ACTIVE, NULL ) );
//
//	// TODO come up with better format
//	FILE* file = fopen( argv[1], "r" );
//	char line[256];
//	char *s, *q;
//	int freq;
//	while( fgets( line, sizeof( line ), file ) ) {
//		/* note that fgets don't strip the terminating \n, checking its
//		   presence would allow to handle lines longer that sizeof(line) */
//		freq = atoi( line );
//		s = strchr( line, ' ' );
//		if( s ) {
//			while( *s == ' ' )
//				s++;
//			q = s;
//			while( *q != '\n' && *q )
//				q++;
//			if( *q == '\n' )
//				*q = '\0';
//			custom_list_append_record( customlist, s, freq );
//		}
//		else {
//			custom_list_append_record( customlist, "", freq );
//		}
//	}
//
//	GtkWidget* treeview = gtk_tree_view_new_with_model( customlist );
//	// gtk_tree_view_set_search_column( GTK_TREE_VIEW( treeview ), COLUMN_DESCRIPTION );
//	g_object_unref( customlist );
//
//	GtkCellRenderer* renderer;
//	GtkTreeViewColumn* col;
//
//	// active col
//	renderer = gtk_cell_renderer_pixbuf_new();
//	col = gtk_tree_view_column_new_with_attributes(
//		"Active", renderer, "pixbuf", CUSTOM_LIST_COL_ACTIVE, NULL );
//	gtk_tree_view_append_column( treeview, col );
//
//	// name col
//	renderer = gtk_cell_renderer_text_new();
//	col = gtk_tree_view_column_new();
//	gtk_tree_view_column_pack_start( col, renderer, TRUE );
//	gtk_tree_view_column_add_attribute( col, renderer, "text", CUSTOM_LIST_COL_NAME );
//	gtk_tree_view_column_set_title( col, "Name" );
//	gtk_tree_view_append_column( GTK_TREE_VIEW( treeview ), col );
//
//	// freq col
//	renderer = gtk_cell_renderer_text_new();
//	col = gtk_tree_view_column_new();
//	gtk_tree_view_column_pack_start( col, renderer, TRUE );
//	gtk_tree_view_column_add_attribute( col, renderer, "text", CUSTOM_LIST_COL_FREQUENCY );
//	gtk_tree_view_column_set_title( col, "freq" );
//	gtk_tree_view_append_column( GTK_TREE_VIEW( treeview ), col );
//
//	// muted col
//	renderer = gtk_cell_renderer_toggle_new();
//	g_signal_connect( renderer, "toggled", G_CALLBACK( muted_toggled ), customlist );
//	col = gtk_tree_view_column_new_with_attributes(
//		"mute", renderer, "active", CUSTOM_LIST_COL_MUTED, NULL );
//	/* set this column to a fixed sizing (of 50 pixels) */
//	gtk_tree_view_column_set_sizing( GTK_TREE_VIEW_COLUMN( col ), GTK_TREE_VIEW_COLUMN_FIXED );
//	gtk_tree_view_column_set_fixed_width( GTK_TREE_VIEW_COLUMN( col ), 50 );
//	gtk_tree_view_append_column( treeview, col );
//
//	g_signal_connect( treeview, "row-activated", G_CALLBACK( on_row_activated ), NULL );
//
//	gtk_container_add( GTK_CONTAINER( sw ), treeview );
//
//	gtk_window_set_default_size( GTK_WINDOW( window ), 700, 700 );
//	gtk_widget_show_all( window );
//	// gtk_widget_show(window);
//	//
//
//	// hack this is here so the twidler can redraw it
//	thetreeview = treeview;
//
//	pthread_t thread;
//	pthread_create( &thread, NULL, twidler, (void*)( customlist ) );
//
//	mainloop = g_main_loop_new( NULL, FALSE );
//	g_main_loop_run( mainloop );
//
//	stop_thread();
//	stop_radio();
//	printf( "waiting for thread\n" );
//	// if(pthread_join(radio_thread, NULL)) {
//	//	fprintf(stderr, "Error joining thread\n");
//	//	return 2;
//
//	//}
//
//	return 0;
//}

//extern int step;
//extern int num_scans;
//extern double *dbms;

#include <gdk/gdk.h>
#include <gtk/gtk.h>

struct gui_data {
	GtkWidget *da;

	int freq_low;
	int freq_high;

	double *dbms;
	int freq_step;
	int num_scans;

	double view_start;
	double view_end;
	double zoom_dbm;

	pthread_mutex_t mutex;
};


int hover_freq = -1;

static gboolean mouse_moved(GtkWidget *widget,GdkEvent *event,gpointer user_data)
{
	struct gui_data *gd = user_data;
	if (event->type==GDK_MOTION_NOTIFY) {
		GdkEventMotion* e=(GdkEventMotion*)event;
		hover_freq = e->x;
		gtk_widget_queue_draw( gd->da );
	}
}

static gboolean mouse_scroll(GtkWidget *widget,GdkEvent *event,gpointer user_data)
{
	struct gui_data *gd = user_data;
	int width = gtk_widget_get_allocated_width (gd->da);

	if (event->type==GDK_SCROLL) {
		GdkEventScroll* e=(GdkEventScroll*)event;
		gdouble x, y;
		gdk_event_get_scroll_deltas( e, &x, &y );

		double ratio = (double) e->x / (double) width;

		double view_range = gd->view_end - gd->view_start;
		double scale = view_range / 10.0;

		if( e->direction == GDK_SCROLL_UP ) {
			printf("zoom in %lf\n", ratio);
			gd->view_start += scale*(ratio);
			gd->view_end -= scale*(1.0-ratio);
		}
		if( e->direction == GDK_SCROLL_DOWN ) {
			printf("zoom out\n");
			gd->view_start -= scale*(ratio);
			gd->view_end += scale*(1.0-ratio);
		}
		if( gd->view_start < 0.0 )
			gd->view_start = 0.0;
		if( gd->view_end > 1.0 )
			gd->view_end = 1.0;
		//printf("scroll dir: %d\n", e->direction );
		//printf("scroll x: (%lf %lf)\n", e->x, x );
		//printf("scroll y: (%lf %lf)\n", e->y, y );
		gtk_widget_queue_draw( gd->da );
	}
}


void fmt_freq(char *s, int freq)
{
	int mhz = freq / 1000000;
	int khz = (freq % 1000000) / 1000;
	sprintf(s, "%d.%03d", mhz, khz);
}

void normalize_signals(double *dbms, int num)
{
	if(num <= 0) { return; }
	double max, min;
	max=min=dbms[0];
	for(int i = 1; i < num; i++) {
		if( dbms[i] > max ) max = dbms[i];
		if( dbms[i] < min ) min = dbms[i];
	}
	double range = max - min;
	for(int i = 1; i < num; i++) {
		dbms[i] = (dbms[i] - min) / range;
	}
	printf("min=%lf max=%lf\n", min, max);
}

double get_dbms_avg(double *dbms, int i1, int i2)
{
	double sum = 0.0;
	int count = 0;
	while(i1 <= i2) {
		sum += dbms[i1];
		count += 1;
		i1++;
	}
	assert(count);
	return sum / count;
}

double get_dbms_max(double *dbms, int i1, int i2)
{
	double max = dbms[i1];
	i1++;
	while(i1 <= i2) {
		if( dbms[i1] > max ) {
			max = dbms[i1];
		}
		i1++;
	}
	return max;
}

static gboolean
checkerboard_draw (GtkWidget *da,
                   cairo_t   *cr,
                   gpointer   data)
{
	struct gui_data *gd = (struct gui_data*) data;
	gint i, j, xcount, width, height;

	pthread_mutex_lock(&(gd->mutex));

	unsigned char buf[32*1024];
	//get_radio_raw_buf( &rs, buf );
	//get_radio_rot_buf( &rs, buf );

	//xcount = 0;
	width = gtk_widget_get_allocated_width (da);
	height = gtk_widget_get_allocated_height (da);
	//i = SPACING;
	//int width_per_step = width / sigs_num_steps;
	//if( width_per_step <= 0 ) {
	//	printf("too small\n");
	//	//return TRUE;
	//}

	double view_range = gd->view_end - gd->view_start;

	//printf("points to plot: %d\n", sigs_num_steps);
	double scale = (double) gd->num_scans*view_range / (double)width;

	//double clamp_range = clamp_max_dbm - clamp_min_dbm;

	int start_i = gd->view_start * gd->num_scans;

	double clamp_min_dbm = -100.0;
	double clamp_max_dbm = 0.0;

	printf("draw %d %d %d %lf\n", width, gd->num_scans, start_i, scale);
	for( i = 0; i < width; i++ ) {
		int i1 = start_i + (int)(scale*i);
		int i2 = start_i + (int)(scale*(i+1));
		if( i1 >= gd->num_scans ) {
			//printf("%d vs %d warning too big\n", ii, gd->num_scans);
			i1 = gd->num_scans-1;
			break;
		}
		if( i2 >= gd->num_scans ) {
			i2 = i1;
		}
		double ss = get_dbms_max(gd->dbms, i1, i2);
		//printf("%lf\n", ss);
		//if( ss < clamp_min_dbm ) { ss = clamp_min_dbm; }
		//if( ss > clamp_max_dbm ) { ss = clamp_max_dbm; }
		//ss -= clamp_min_dbm;
		//ss /= (clamp_max_dbm - clamp_min_dbm);

		ss *= 200.0;
		//ss *= zoom_dbm;

		int y = 200 - ss; // * 300.0;

		cairo_set_source_rgb( cr, 0, 0, 0 );

		cairo_rectangle( cr, i, y, 3, 3);
		cairo_fill (cr);
	}
	//cairo_select_font_face( cr, "Purisa",
	//		CAIRO_FONT_SLANT_NORMAL,
	//		CAIRO_FONT_WEIGHT_BOLD);

	//cairo_set_font_size(cr, 16);
	cairo_set_font_size(cr, 16);

	//int tick_width = 100;
	//int num_ticks = width / tick_width;
	//if( num_ticks >= 2 ) {
	//	for(int i = 0; i < num_ticks; i++ ) {
	//		int x = i * tick_width;
	//		char s[1024];
	//		int freq = sigs_low_freq + (x/width_per_step)*sigs_step;
	//		sprintf(s, "%d", freq);

	//		cairo_move_to(cr, x, 100);
	//		cairo_show_text(cr, s);
	//	}
	//}
	if( 0 <= hover_freq && hover_freq < width ) {
		int i = start_i + (int)((double)hover_freq * scale);
		int freq = gd->freq_low + i*gd->freq_step;
		char s[1024];
		fmt_freq(s, freq);

		cairo_move_to(cr, 50, 50);
		cairo_show_text(cr, s);
	}

	pthread_mutex_unlock(&(gd->mutex));

	/* return TRUE because we've handled this event, so no
	 * further processing is required.
	 */
	return TRUE;
}

void plot_dbms(struct gui_data *gd) //int low_freq, int step, int num_steps, double *dbms)
{
	//sigs_low_freq = low_freq;
	//sigs_num_steps = num_steps;
	//sigs_step = step;
	//sigs = dbms;
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

	gd->da = gtk_drawing_area_new ();
	/* set a minimum size */
	gtk_widget_set_size_request (gd->da, 100, 100);

	gtk_container_add (GTK_CONTAINER (frame), gd->da);

	g_signal_connect (gd->da, "draw", G_CALLBACK(checkerboard_draw), gd);
    g_signal_connect (gd->da, "motion-notify-event", G_CALLBACK(mouse_moved), gd);
    g_signal_connect (gd->da, "scroll-event", G_CALLBACK(mouse_scroll), gd);
	gtk_widget_set_events(gd->da, GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK );

	gtk_window_set_default_size( GTK_WINDOW( window ), 3000, 700 );
	gtk_widget_show_all( window );

	GMainLoop* mainloop = NULL;
	mainloop = g_main_loop_new( NULL, FALSE );
	g_main_loop_run( mainloop );

	return 0;
}

void dbms_update(double *dbms, int start_freq, int freq_step, int num_steps, void *user_data)
{
	struct gui_data *gd = user_data;

	usleep(100000);

	pthread_mutex_lock(&(gd->mutex));
	memcpy(gd->dbms, dbms, num_steps*sizeof(double));

	//printf("%d %d\n", freq_step, num_steps);
	gd->freq_step = freq_step;
	gd->num_scans = num_steps;

	normalize_signals(gd->dbms, num_steps);

	pthread_mutex_unlock(&(gd->mutex));


	if( gd->da ) {
		gtk_widget_queue_draw( gd->da );
	}

	//double min, max;
	//min = max = gd->dbms[0];
	//for(int i = 1; i < num_steps; i++ ) {
	//	if( gd->dbms[i] > max ) { max = gd->dbms[i]; }
	//	if( gd->dbms[i] < min ) { min = gd->dbms[i]; }
	//}
	//printf("%lf %lf\n", min, max);

}

int main(int argc, char **argv)
{
	gtk_init( &argc, &argv );

	struct gui_data gd;
	memset(&gd, 0, sizeof(struct gui_data));
	pthread_mutex_init(&gd.mutex, NULL);
	gd.freq_low = 159000000;
	gd.freq_high = 165000000;

	if( argc == 3 ) {
		gd.freq_low = atoi(argv[1]) * 1000000;
		gd.freq_high = atoi(argv[2]) * 1000000;
	}


	size_t n = gd.freq_high - gd.freq_low; // TODO fix size
	gd.dbms = malloc(n*sizeof(double));
	assert(gd.dbms);

	gd.view_start = 0.0;
	gd.view_end = 1.0;
	gd.zoom_dbm = 1.0;

	struct radio_scanner *rs;
	init_radio(&rs, dbms_update, &gd);

	radio_sample(rs, gd.freq_low, gd.freq_high);

	plot_dbms(&gd); //freq_low, step, bin_len*num_scans, dbms);

	stop_radio(rs);

	return 0;
}

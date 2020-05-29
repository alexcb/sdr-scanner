#include "custom-list.h"
#include "rtl_fm_scanner.h"
#include "should_stop.h"

#include <assert.h>

#include <libayatana-appindicator/app-indicator.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-glib/server.h>

GMainLoop* mainloop = NULL;

#define ICON_ACTIVE "/home/alex/gh/alexcb/sdr-scanner/radio-active.svg"
#define ICON_SCANNING "/home/alex/gh/alexcb/sdr-scanner/radio-scanning.svg" // white
#define ICON_MUTED "/home/alex/gh/alexcb/sdr-scanner/radio-muted.svg"
#define ICON_BLACK "/home/alex/gh/alexcb/sdr-scanner/radio-black.svg"

GtkWidget* window = NULL;
GtkWidget* thetreeview = NULL;
AppIndicator* ci = NULL;

struct radio_scanner* rs = NULL;

static void muted_toggled( GtkCellRendererToggle* cell, gchar* path_str, gpointer data )
{
	CustomList* model = (CustomList*)data;

	GtkTreePath* path = gtk_tree_path_new_from_string( path_str );
	gint selected_track = gtk_tree_path_get_indices( path )[0];

	printf( "toggle mute: %d\n", selected_track );

	CustomRecord* record;
	record = model->rows[selected_track];
	record->muted = !record->muted;

	gtk_tree_path_free( path );
}

static void mute_item_clicked_cb( GtkWidget* widget, gpointer data )
{
	struct radio_scanner* rs = (struct radio_scanner*)data;
	if( gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( widget ) ) ) {
		printf( "muted\n" );
		set_radio_volume( rs, 0 );
		app_indicator_set_icon( ci, ICON_MUTED );
	}
	else {
		printf( "active\n" );
		set_radio_volume( rs, 100 );
		app_indicator_set_icon( ci, ICON_SCANNING );
	}
}

static void scan_button_toggled( GtkWidget* widget, gpointer data )
{
	gboolean toggled = gtk_toggle_button_get_active( widget );
	if( toggled ) {
		printf( "scan should be on\n" );
	} else {
		printf( "scan should be off\n" );
	}
}

static void channels_item_clicked_cb( GtkWidget* widget, gpointer data )
{
	printf( "raise it\n" );
	// gtk_window_maximize( window );
	gtk_window_present( GTK_WINDOW( window ) );
}

static void quit_item_clicked_cb( GtkWidget* widget, gpointer data )
{
	g_main_loop_quit( mainloop );
}

// this thread controlls the scanner freq selection + telling the UI which freq is selected
static void* twidler( void* arg )
{

	//int freqs[] = {162475000, 162400000};

	//{
	//	int i = 0;
	//	for(;;) {
	//		printf("go %d\n", freqs[i%2]);
	//		set_radio_freq( &rs, freqs[i%2] );
	//			usleep( 1000 );
	//			printf( "signal %d\n", get_radio_signal_strength( &rs ) );
	//		i++;
	//		usleep( 1000000 );
	//	}
	//}

	
	CustomList* custom_list = (CustomList*)arg;
	int i = 0;
	int j = 0;
	bool needs_tune = true;
	GtkTreeIter iter;
	for( ;; ) {
		for( ;; ) {
			if( needs_tune ) {
			printf("%d\n", custom_list->rows[j]->frequency);
			set_radio_freq( &rs, custom_list->rows[j]->frequency );
			needs_tune = false;
			}

			custom_list->current_channel = j;
			gtk_widget_queue_draw( thetreeview );
			//usleep( 100000 );
			sleep(1);
			int sig = get_radio_signal_strength( &rs );
			if( sig > 200 ) {
				printf("found signal\n");
				continue;
			}
			j = (j+1) % custom_list->num_rows;
			needs_tune = true;
		}
		// if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( model ), &iter ) ) {
		//	do {
		//		gtk_list_store_set(
		//			model, &iter, COLUMN_ACTIVE_ICON, i == j ? active_freq_pixbuf : NULL, -1 );
		//		j++;
		//	} while( gtk_tree_model_iter_next( GTK_TREE_MODEL( model ), &iter ) );
		//}
		// i = ( i + 1 ) % 2;
		usleep( 100000 );
	}
}

void on_row_activated( GtkTreeView* tree_view,
					   GtkTreePath* path,
					   GtkTreeViewColumn* column,
					   gpointer user_data )
{
	gint selected_track = gtk_tree_path_get_indices( path )[0];
	printf( "activate %d\n", selected_track );
}

// typedef struct {
//	GtkApplicationWindow parent_instance;
//
//	int current_width;
//	int current_height;
//	bool is_maximized;
//	bool is_fullscreen;
//} MyApplicationWindow;
//
// static void my_application_window_store_state (MyApplicationWindow *self)
//{
//	GSettings *settings = g_settings_new ("ca.mofo.sdr-scanner");
//
//	g_settings_set_int (settings, "width", self->current_width);
//	g_settings_set_int (settings, "height", self->current_height);
//	g_settings_set_boolean (settings, "is-maximized", self->is_maximized);
//	g_settings_set_boolean (settings, "is-fullscreen", self->is_fullscreen);
//}
//
// static void on_window_destroy (GtkWidget *widget)
//{
//  MyApplicationWindow *app_window = MY_APPLICATION_WINDOW (widget);
//
//  // store the state here
//  my_application_window_store_state (app_window);
//
//  // chain up to the parent's implementation
//  GTK_WIDGET_CLASS (my_application_window_parent_class)->destroy (widget);
//}

int main( int argc, char** argv )
{
	GtkWidget* menu = NULL;

	if( argc != 2 ) {
		fprintf( stderr, "usage: %s <freq.txt>\n", argv[0] );
		return 1;
	}

	gtk_init( &argc, &argv );

	//int cbc = 88100000;
	//int vhf_marine_1 = 162475000;
	//int vhf_marine_2 = 162400000;

	//int freqs[] = {162475000, 162400000};

	// start radio thread
	stop_thread_init();
	int res = init_radio( &rs );
	if( res ) {
		fprintf( stderr, "failed to start thread\n" );
		// return 1;
	}

	ci = app_indicator_new(
		"sdr-scanner", ICON_SCANNING, APP_INDICATOR_CATEGORY_APPLICATION_STATUS );

	g_assert( IS_APP_INDICATOR( ci ) );
	g_assert( G_IS_OBJECT( ci ) );

	app_indicator_set_status( ci, APP_INDICATOR_STATUS_ACTIVE );
	// app_indicator_set_attention_icon_full( ci, "tray-new-im", "asdfasdf" );
	// app_indicator_set_title( ci, "Test Inidcator" );
	// app_indicator_set_icon( ci, "/usr/share/icons/ubuntu-mono-dark/status/24/nm-signal-100.svg"
	// ); //"/home/alex/gh/alexcb/sdr-scanner/radio.svg" );

	menu = gtk_menu_new();

	// mute
	GtkWidget* mute_item = gtk_check_menu_item_new_with_label( "mute" );
	g_signal_connect( mute_item, "activate", G_CALLBACK( mute_item_clicked_cb ), rs );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), mute_item );
	gtk_widget_show( mute_item );

	// channels
	GtkWidget* channels_item = gtk_menu_item_new_with_label( "channels" );
	g_signal_connect( channels_item, "activate", G_CALLBACK( channels_item_clicked_cb ), rs );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), channels_item );
	gtk_widget_show( channels_item );

	// quit
	GtkWidget* quit_item = gtk_menu_item_new_with_label( "quit" );
	g_signal_connect( quit_item, "activate", G_CALLBACK( quit_item_clicked_cb ), NULL );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), quit_item );
	gtk_widget_show( quit_item );

	app_indicator_set_menu( ci, GTK_MENU( menu ) );

	window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
	gtk_window_set_icon_from_file( GTK_WINDOW( window ), ICON_SCANNING, NULL );

	GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 8 );
	gtk_container_add( GTK_CONTAINER( window ), vbox );

	//GtkWidget* label = gtk_label_new( "hello scanner" );
	//gtk_box_pack_start( GTK_BOX( vbox ), label, FALSE, FALSE, 0 );

	GtkWidget* scan_button = gtk_toggle_button_new_with_label( "scan" );
	g_signal_connect( scan_button, "toggled", G_CALLBACK( scan_button_toggled ), rs );
	gtk_box_pack_start( GTK_BOX( vbox ), scan_button, FALSE, FALSE, 0 );

	GtkWidget* sw = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( sw ), GTK_SHADOW_ETCHED_IN );
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW( sw ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_box_pack_start( GTK_BOX( vbox ), sw, TRUE, TRUE, 0 );

	CustomList* customlist;
	customlist = custom_list_new();

	assert( customlist->active_pixbuf = gdk_pixbuf_new_from_file( ICON_BLACK, NULL ) );
	assert( customlist->audio_pixbuf = gdk_pixbuf_new_from_file( ICON_ACTIVE, NULL ) );

	// TODO come up with better format
	FILE* file = fopen( argv[1], "r" );
	char line[256];
	char *s, *q;
	int freq;
	while( fgets( line, sizeof( line ), file ) ) {
		/* note that fgets don't strip the terminating \n, checking its
		   presence would allow to handle lines longer that sizeof(line) */
		freq = atoi( line );
		s = strchr( line, ' ' );
		if( s ) {
			while( *s == ' ' )
				s++;
			q = s;
			while( *q != '\n' && *q )
				q++;
			if( *q == '\n' )
				*q = '\0';
			custom_list_append_record( customlist, s, freq );
		}
		else {
			custom_list_append_record( customlist, "", freq );
		}
	}

	GtkWidget* treeview = gtk_tree_view_new_with_model( customlist );
	// gtk_tree_view_set_search_column( GTK_TREE_VIEW( treeview ), COLUMN_DESCRIPTION );
	g_object_unref( customlist );

	GtkCellRenderer* renderer;
	GtkTreeViewColumn* col;

	// active col
	renderer = gtk_cell_renderer_pixbuf_new();
	col = gtk_tree_view_column_new_with_attributes(
		"Active", renderer, "pixbuf", CUSTOM_LIST_COL_ACTIVE, NULL );
	gtk_tree_view_append_column( treeview, col );

	// name col
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start( col, renderer, TRUE );
	gtk_tree_view_column_add_attribute( col, renderer, "text", CUSTOM_LIST_COL_NAME );
	gtk_tree_view_column_set_title( col, "Name" );
	gtk_tree_view_append_column( GTK_TREE_VIEW( treeview ), col );

	// freq col
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start( col, renderer, TRUE );
	gtk_tree_view_column_add_attribute( col, renderer, "text", CUSTOM_LIST_COL_FREQUENCY );
	gtk_tree_view_column_set_title( col, "freq" );
	gtk_tree_view_append_column( GTK_TREE_VIEW( treeview ), col );

	// muted col
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect( renderer, "toggled", G_CALLBACK( muted_toggled ), customlist );
	col = gtk_tree_view_column_new_with_attributes(
		"mute", renderer, "active", CUSTOM_LIST_COL_MUTED, NULL );
	/* set this column to a fixed sizing (of 50 pixels) */
	gtk_tree_view_column_set_sizing( GTK_TREE_VIEW_COLUMN( col ), GTK_TREE_VIEW_COLUMN_FIXED );
	gtk_tree_view_column_set_fixed_width( GTK_TREE_VIEW_COLUMN( col ), 50 );
	gtk_tree_view_append_column( treeview, col );

	g_signal_connect( treeview, "row-activated", G_CALLBACK( on_row_activated ), NULL );

	gtk_container_add( GTK_CONTAINER( sw ), treeview );

	gtk_window_set_default_size( GTK_WINDOW( window ), 700, 700 );
	gtk_widget_show_all( window );
	// gtk_widget_show(window);
	//

	// hack this is here so the twidler can redraw it
	thetreeview = treeview;

	pthread_t thread;
	pthread_create( &thread, NULL, twidler, (void*)( customlist ) );

	mainloop = g_main_loop_new( NULL, FALSE );
	g_main_loop_run( mainloop );

	stop_thread();
	stop_radio();
	printf( "waiting for thread\n" );
	// if(pthread_join(radio_thread, NULL)) {
	//	fprintf(stderr, "Error joining thread\n");
	//	return 2;

	//}

	return 0;
}

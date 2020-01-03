#include "custom-list.h"
#include "rtl_fm_scanner.h"
#include "should_stop.h"

#include <assert.h>

#include <libayatana-appindicator/app-indicator.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-glib/server.h>

GMainLoop* mainloop = NULL;

#define ICON_ACTIVE "/home/alex/gh/alexcb/sdr-scanner/radio-active.svg"
#define ICON_SCANNING "/home/alex/gh/alexcb/sdr-scanner/radio-scanning.svg"
#define ICON_MUTED "/home/alex/gh/alexcb/sdr-scanner/radio-muted.svg"
#define ICON_BLACK "/home/alex/gh/alexcb/sdr-scanner/radio-black.svg"

GtkWidget* window = NULL;
static GtkTreeModel* model = NULL;
AppIndicator* ci = NULL;

GdkPixbuf* active_freq_pixbuf = NULL;

typedef struct
{
	const gboolean active;
	const gboolean muted;
	const guint frequency;
	const guint last_active;
	const gchar* description;
} Channel;

static Channel data[] = {
	{TRUE, FALSE, 146520000, 0, "2m calling"},
	{FALSE, FALSE, 446000000, 0, "70cm calling"},
};

enum
{
	COLUMN_ACTIVE_ICON,
	COLUMN_MUTED,
	COLUMN_FREQ,
	COLUMN_LAST_ACTIVE,
	COLUMN_DESCRIPTION,
	NUM_COLUMNS
};

static GtkTreeModel* create_model( void )
{
	gint i = 0;
	GtkListStore* store;
	GtkTreeIter iter;

	/* create list store */
	store = gtk_list_store_new( NUM_COLUMNS,
								GDK_TYPE_PIXBUF, // active icon
								G_TYPE_BOOLEAN, // muted
								G_TYPE_UINT, // freq
								G_TYPE_UINT, // last active
								G_TYPE_STRING // desc
	);

	/* add data to the list store */
	for( i = 0; i < G_N_ELEMENTS( data ); i++ ) {
		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store,
							&iter,
							COLUMN_ACTIVE_ICON,
							i == 0 ? active_freq_pixbuf : NULL,
							COLUMN_MUTED,
							data[i].muted,
							COLUMN_FREQ,
							data[i].frequency,
							COLUMN_LAST_ACTIVE,
							data[i].last_active,
							COLUMN_DESCRIPTION,
							data[i].description,
							-1 );
	}

	return GTK_TREE_MODEL( store );
}

static void muted_toggled( GtkCellRendererToggle* cell, gchar* path_str, gpointer data )
{
	GtkTreeModel* model = (GtkTreeModel*)data;
	GtkTreeIter iter;
	GtkTreePath* path = gtk_tree_path_new_from_string( path_str );
	gboolean muted;

	/* get toggled iter */
	gtk_tree_model_get_iter( model, &iter, path );
	gtk_tree_model_get( model, &iter, COLUMN_MUTED, &muted, -1 );

	/* do something with the value */
	muted ^= 1;

	/* set new value */
	gtk_list_store_set( GTK_LIST_STORE( model ), &iter, COLUMN_MUTED, muted, -1 );

	/* clean up */
	gtk_tree_path_free( path );
}

static void add_columns( GtkTreeView* treeview )
{
	GtkCellRenderer* renderer;
	GtkTreeViewColumn* column;
	GtkTreeModel* model = gtk_tree_view_get_model( treeview );

	/* column for active icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
		"Active", renderer, "pixbuf", COLUMN_ACTIVE_ICON, NULL );
	gtk_tree_view_append_column( treeview, column );

	/* column for muted toggles */
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect( renderer, "toggled", G_CALLBACK( muted_toggled ), model );

	column =
		gtk_tree_view_column_new_with_attributes( "mute", renderer, "active", COLUMN_MUTED, NULL );

	/* set this column to a fixed sizing (of 50 pixels) */
	gtk_tree_view_column_set_sizing( GTK_TREE_VIEW_COLUMN( column ), GTK_TREE_VIEW_COLUMN_FIXED );
	gtk_tree_view_column_set_fixed_width( GTK_TREE_VIEW_COLUMN( column ), 50 );
	gtk_tree_view_append_column( treeview, column );

	/* column for bug numbers */
	renderer = gtk_cell_renderer_text_new();
	column =
		gtk_tree_view_column_new_with_attributes( "Freq", renderer, "text", COLUMN_FREQ, NULL );
	gtk_tree_view_column_set_sort_column_id( column, COLUMN_FREQ );
	gtk_tree_view_append_column( treeview, column );

	/* column for last heard */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		"last heard", renderer, "text", COLUMN_LAST_ACTIVE, NULL );
	gtk_tree_view_column_set_sort_column_id( column, COLUMN_FREQ );
	gtk_tree_view_append_column( treeview, column );

	/* column for description */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		"Description", renderer, "text", COLUMN_DESCRIPTION, NULL );
	gtk_tree_view_column_set_sort_column_id( column, COLUMN_DESCRIPTION );
	gtk_tree_view_append_column( treeview, column );
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

static void* twidler( void* arg )
{
	int i = 0;
	GtkTreeIter iter;
	for( ;; ) {
		int j = 0;
		// TODO change this twidler to interact with custom-list; which is ultimately gojng to be
		// the scanner thread if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL( model ), &iter ) ) {
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

int main( int argc, char** argv )
{
	GtkWidget* menu = NULL;

	if( argc != 2 ) {
		fprintf( stderr, "usage: %s <freq.txt>\n", argv[0] );
		return 1;
	}

	gtk_init( &argc, &argv );

	// start radio thread
	stop_thread_init();
	struct radio_scanner* rs;
	int res = init_radio( &rs, 162475000 );
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

	GtkWidget* label = gtk_label_new( "hello scanner" );
	gtk_box_pack_start( GTK_BOX( vbox ), label, FALSE, FALSE, 0 );

	GtkWidget* sw = gtk_scrolled_window_new( NULL, NULL );
	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( sw ), GTK_SHADOW_ETCHED_IN );
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW( sw ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
	gtk_box_pack_start( GTK_BOX( vbox ), sw, TRUE, TRUE, 0 );

	// must be done before create_model due to global
	active_freq_pixbuf = gdk_pixbuf_new_from_file( ICON_BLACK, NULL );
	assert( active_freq_pixbuf );

	CustomList* customlist;
	customlist = custom_list_new();

	// TODO come up with better format
	FILE* file = fopen( argv[1], "r" );
	char line[256];
	char* s, *q;
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

	// model = create_model();
	GtkWidget* treeview = gtk_tree_view_new_with_model( customlist );
	// gtk_tree_view_set_search_column( GTK_TREE_VIEW( treeview ), COLUMN_DESCRIPTION );
	g_object_unref( customlist );

	GtkCellRenderer* renderer;
	GtkTreeViewColumn* col;
	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new();

	gtk_tree_view_column_pack_start( col, renderer, TRUE );
	gtk_tree_view_column_add_attribute( col, renderer, "text", CUSTOM_LIST_COL_NAME );
	gtk_tree_view_column_set_title( col, "Name" );
	gtk_tree_view_append_column( GTK_TREE_VIEW( treeview ), col );

	renderer = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start( col, renderer, TRUE );
	gtk_tree_view_column_add_attribute( col, renderer, "text", CUSTOM_LIST_COL_FREQUENCY );
	gtk_tree_view_column_set_title( col, "freq" );
	gtk_tree_view_append_column( GTK_TREE_VIEW( treeview ), col );

	g_signal_connect( treeview, "row-activated", G_CALLBACK( on_row_activated ), NULL );

	gtk_container_add( GTK_CONTAINER( sw ), treeview );
	// add_columns( GTK_TREE_VIEW( treeview ) );

	gtk_widget_show_all( window );
	// gtk_widget_show(window);

	pthread_t thread;
	pthread_create( &thread, NULL, twidler, (void*)( NULL ) );

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

#include "rtl_fm_scanner.h"
#include "should_stop.h"

#include <libayatana-appindicator/app-indicator.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-glib/server.h>

GMainLoop* mainloop = NULL;


void *radio_worker(void *x_void_ptr)
{
	printf("starting background thread\n");
	int res = init_radio( 162475000 );
	printf("radio thread exited with %d\n", res);
	int i = 0;
	while( !should_thread_stop() ) {
		i = (i%100) + 1;
		printf("hello %d\n", i);
		sleep(1);
	}
	return NULL;
}

static void mute_item_clicked_cb( GtkWidget* widget, gpointer data )
{
	if( gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM( widget ) ) ) {
		printf("muted\n");
	} else {
		printf("active\n");
	}
}

static void quit_item_clicked_cb( GtkWidget* widget, gpointer data )
{
	g_main_loop_quit( mainloop );
}

int main( int argc, char** argv )
{
	GtkWidget* menu = NULL;
	AppIndicator* ci = NULL;

	gtk_init( &argc, &argv );

	ci = app_indicator_new(
		"example-simple-client", "indicator-messages", APP_INDICATOR_CATEGORY_APPLICATION_STATUS );

	g_assert( IS_APP_INDICATOR( ci ) );
	g_assert( G_IS_OBJECT( ci ) );

	app_indicator_set_status( ci, APP_INDICATOR_STATUS_ACTIVE );
	app_indicator_set_attention_icon_full(
		ci, "indicator-messages-new", "System Messages Icon Highlighted" );
	app_indicator_set_title( ci, "Test Inidcator" );

	menu = gtk_menu_new();

	// mute
	GtkWidget* mute_item = gtk_check_menu_item_new_with_label( "mute" );
	g_signal_connect( mute_item, "activate", G_CALLBACK( mute_item_clicked_cb ), NULL );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), mute_item );
	gtk_widget_show( mute_item );

	// quit
	GtkWidget* quit_item = gtk_menu_item_new_with_label( "quit" );
	g_signal_connect( quit_item, "activate", G_CALLBACK( quit_item_clicked_cb ), NULL );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), quit_item );
	gtk_widget_show( quit_item );

	app_indicator_set_menu( ci, GTK_MENU( menu ) );

	// start radio thread
	stop_thread_init();
	int res = init_radio( 162475000 );
	if( res ) {
		fprintf(stderr, "failed to start thread\n");
		return 1;
	}

	mainloop = g_main_loop_new( NULL, FALSE );
	g_main_loop_run( mainloop );

	stop_thread();
	stop_radio();
	printf("waiting for thread\n");
	//if(pthread_join(radio_thread, NULL)) {
	//	fprintf(stderr, "Error joining thread\n");
	//	return 2;

	//}

	return 0;
}

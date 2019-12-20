#include "rtl_fm_scanner.h"
#include "should_stop.h"

#include <libayatana-appindicator/app-indicator.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-glib/server.h>

GMainLoop* mainloop = NULL;


static void mute_item_clicked_cb( GtkWidget* widget, gpointer data )
{
	struct radio_scanner *rs = (struct radio_scanner*) data;
	if( gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM( widget ) ) ) {
		printf("muted\n");
		set_radio_volume(rs, 0);
	} else {
		printf("active\n");
		set_radio_volume(rs, 100);
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

	// start radio thread
	stop_thread_init();
	struct radio_scanner *rs;
	int res = init_radio( &rs, 162475000 );
	if( res ) {
		fprintf(stderr, "failed to start thread\n");
		return 1;
	}

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
	g_signal_connect( mute_item, "activate", G_CALLBACK( mute_item_clicked_cb ), rs );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), mute_item );
	gtk_widget_show( mute_item );

	// quit
	GtkWidget* quit_item = gtk_menu_item_new_with_label( "quit" );
	g_signal_connect( quit_item, "activate", G_CALLBACK( quit_item_clicked_cb ), NULL );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), quit_item );
	gtk_widget_show( quit_item );

	app_indicator_set_menu( ci, GTK_MENU( menu ) );

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

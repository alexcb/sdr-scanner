/*
A small piece of sample code demonstrating a very simple application
with an indicator.

Copyright 2009 Canonical Ltd.

Authors:
	Ted Gould <ted@canonical.com>

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License version 3, as published
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranties of
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <libayatana-appindicator/app-indicator.h>
#include <libdbusmenu-glib/menuitem.h>
#include <libdbusmenu-glib/server.h>

GMainLoop* mainloop = NULL;

//#define LOCAL_ICON "/home/alex/libayatana-appindicator/example/simple-client-test-icon.png"
// static void local_icon_toggle_cb( GtkWidget* widget, gpointer data )
//{
//	AppIndicator* ci = APP_INDICATOR( data );
//
//	if( gtk_check_menu_item_get_active( GTK_CHECK_MENU_ITEM( widget ) ) ) {
//		app_indicator_set_icon_full( ci, LOCAL_ICON, "Local Icon" );
//	}
//	else {
//		app_indicator_set_icon_full( ci, "indicator-messages", "System Icon" );
//	}
//
//	return;
//}

int thread_stop_issued = 0;
pthread_mutex_t thread_stop_mutex;

int should_thread_stop(void) {
  int ret = 0;
  pthread_mutex_lock(&thread_stop_mutex);
  ret = thread_stop_issued;
  pthread_mutex_unlock(&thread_stop_mutex);
  return ret;
}

void stop_thread() {
  pthread_mutex_lock(&thread_stop_mutex);
  thread_stop_issued = 1;
  pthread_mutex_unlock(&thread_stop_mutex);
}

void *radio_worker(void *x_void_ptr)
{
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
	pthread_t radio_thread;

	if(pthread_create(&radio_thread, NULL, radio_worker, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}


	mainloop = g_main_loop_new( NULL, FALSE );
	g_main_loop_run( mainloop );

	stop_thread();
	printf("waiting for thread\n");
	if(pthread_join(radio_thread, NULL)) {
		fprintf(stderr, "Error joining thread\n");
		return 2;

	}


	return 0;
}

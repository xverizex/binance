#include "view_graph.h"
#include "sql.h"
#include <stdlib.h>

struct _ViewGraphPrivate
{
	GdkWindow *window;
};

const int WIDTH = 400;
const int HEIGHT = 400;

G_DEFINE_TYPE (ViewGraph, view_graph, GTK_TYPE_WIDGET);

static void view_graph_get_preferred_width ( GtkWidget *widget, int *min_w, int *nat_w ) {
	*min_w = *nat_w = WIDTH;
}

static void view_graph_get_preferred_height ( GtkWidget *widget, int *min_h, int *nat_h ) {
	*min_h = *nat_h = HEIGHT;
}

static void view_graph_size_allocate ( GtkWidget *widget, GtkAllocation *al ) {
	ViewGraphPrivate *priv;
	priv = VIEW_GRAPH ( widget )->priv;

	gtk_widget_set_allocation ( widget, al );
	if ( gtk_widget_get_realized ( widget ) ) {
		gdk_window_move_resize ( priv->window, al->x, al->y, al->width, al->height );
	}
}

static void view_graph_realize ( GtkWidget *widget ) {
	ViewGraphPrivate *priv;
	priv = VIEW_GRAPH ( widget )->priv;
	GtkAllocation al;
	GdkWindowAttr attrs;
	int attrs_mask;

	gtk_widget_set_realized ( widget, TRUE );

	gtk_widget_get_allocation ( widget, &al );

	attrs.x = al.x;
	attrs.y = al.y;
	attrs.width = al.width;
	attrs.height = al.height;
	attrs.window_type = GDK_WINDOW_CHILD;
	attrs.wclass = GDK_INPUT_OUTPUT;
	attrs.event_mask = gtk_widget_get_events ( widget ) | GDK_EXPOSURE_MASK;

	attrs_mask = GDK_WA_X | GDK_WA_Y;

	priv->window = gdk_window_new ( gtk_widget_get_parent_window ( widget ), &attrs, attrs_mask );
	gdk_window_set_user_data ( priv->window, widget );
	gtk_widget_set_window ( widget, priv->window );
}

static gboolean view_graph_draw ( GtkWidget *widget, cairo_t *cr ) {
	float color = 0x3c / 255.0;
	cairo_set_source_rgb ( cr, color, color, color );
	cairo_paint ( cr );

	return FALSE;
}

static void view_graph_init ( ViewGraph *view_graph ) {
  //view_graph->priv = G_TYPE_INSTANCE_GET_PRIVATE ( view_graph, VIEW_TYPE_GRAPH, ViewGraphPrivate );

  /* TODO: Add initialization code here */
	ViewGraphPrivate *priv;
	priv = calloc ( 1, sizeof ( ViewGraphPrivate ) );
	view_graph->priv = priv;
	gtk_widget_set_has_window ( GTK_WIDGET ( view_graph ), TRUE );

}

static void view_graph_finalize ( GObject *object ) {
  /* TODO: Add deinitialization code here */
  G_OBJECT_CLASS ( view_graph_parent_class )->finalize ( object );
}

static void view_graph_class_init ( ViewGraphClass *klass ) {
  GObjectClass *object_class = G_OBJECT_CLASS ( klass );
  object_class->finalize = view_graph_finalize;

  GtkWidgetClass *w_class = GTK_WIDGET_CLASS ( klass );
  w_class->get_preferred_width = view_graph_get_preferred_width;
  w_class->get_preferred_height = view_graph_get_preferred_height;
  w_class->realize = view_graph_realize;
  w_class->size_allocate = view_graph_size_allocate;
  w_class->draw = view_graph_draw;
}

GtkWidget *view_graph_new ( ) {
	return g_object_new ( VIEW_TYPE_GRAPH, NULL );
}

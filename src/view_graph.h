#ifndef _VIEW_GRAPH_H_
#define _VIEW_GRAPH_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VIEW_TYPE_GRAPH (view_graph_get_type())
#define VIEW_GRAPH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), VIEW_TYPE_GRAPH, ViewGraph))
#define VIEW_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), VIEW_TYPE_GRAPH, ViewGraphClass))
#define VIEW_IS_GRAPH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIEW_TYPE_GRAPH))
#define VIEW_IS_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), VIEW_TYPE_GRAPH))
#define VIEW_GRAPH_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), VIEW_TYPE_GRAPH, ViewGraphClass))

typedef struct _ViewGraphClass ViewGraphClass;
typedef struct _ViewGraph ViewGraph;
typedef struct _ViewGraphPrivate ViewGraphPrivate;


struct _ViewGraphClass
{
  GtkWidgetClass parent_class;
};

struct _ViewGraph
{
  GtkWidget parent_instance;
  ViewGraphPrivate *priv;
};

GType view_graph_get_type (void) G_GNUC_CONST;
GtkWidget *view_graph_new ( void );

G_END_DECLS

#endif  /* _VIEW_GRAPH_H_ */

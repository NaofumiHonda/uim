/*
 *  $Id:$
 *  Copyright (c) 2003,2004 Masahito Omote <omote@utyuuzin.net>
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of authors nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */

#include <gtk/gtk.h>

#include "wordlist_view.h"
#include "uim/config.h"
#include "uim/gettext.h"


#include <stdlib.h>
#include <string.h>

static void wordlist_view_class_init   (WordlistViewClass *klass);
static void wordlist_view_init         (WordlistView *view);
static void wordlist_view_finalize     (GObject *object);
static void wordlist_view_destroy      (GtkObject *object);

static void wordlist_view_set_property (GObject *object,
					guint prop_id,
					const GValue *value,
					GParamSpec *pspec);
static void wordlist_view_get_property (GObject *object,
					guint prop_id,
					GValue *value,
					GParamSpec *pspec);

/* callbacks */
static void wordlist_view_callback_cell_edited (GtkCellRendererText *renderer,
						const gchar *path_string,
						const gchar *new_text,
						WordlistView *view);

static GtkScrolledWindowClass *parent_class = NULL;

enum {
    PROP_0,
    PROP_CCLASS_CODE_SHOW,
    PROP_FREQ_SHOW,
    PROP_OKURI_SHOW,
    PROP_EDITABLE,
    PROP_SELECTION_MODE,
};

typedef enum {
    WORDLIST_WORD_TYPE = 0,
    WORDLIST_PHON,
    WORDLIST_DESC,
    WORDLIST_CCLASS_CODE,
    WORDLIST_FREQ,
    WORDLIST_OKURI,
    WORDLIST_EDITABLE,
    WORDLIST_N_COLUMNS
} WordlistItems;

GType wordlist_view_get_type(void) {
    static GType type = 0;

    if(type == 0) {
	static const GTypeInfo info = {
	    sizeof(WordlistViewClass),
	    NULL, /* base_init */
	    NULL, /* base_finalize */
	    (GClassInitFunc)wordlist_view_class_init,
	    NULL, /* class_finalize */
	    NULL, /* class_data */
	    sizeof(WordlistView),
	    0, /* n_preallocs */
	    (GInstanceInitFunc)wordlist_view_init /* instance_init */
	};
	type = g_type_register_static(GTK_TYPE_SCROLLED_WINDOW,
				      "WordlistView", &info, 0);
    }
    return type;
}

static void wordlist_view_class_init(WordlistViewClass *klass) {
    GObjectClass   *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->get_property = wordlist_view_get_property;
    gobject_class->set_property = wordlist_view_set_property;
    gobject_class->finalize = wordlist_view_finalize;
    object_class->destroy = wordlist_view_destroy;

    g_object_class_install_property(gobject_class,
				    PROP_CCLASS_CODE_SHOW,
				    g_param_spec_boolean("cclass_code_show",
							 _("Show conjection class code column"),
							 _("to be written"),
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

    g_object_class_install_property(gobject_class,
				    PROP_FREQ_SHOW,
				    g_param_spec_boolean("freq_show",
							 _("Show freq column"),
							 _("to be written"),
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(gobject_class,
				    PROP_OKURI_SHOW,
				    g_param_spec_boolean("okuri_show",
							 _("Show okuri column"),
							 _("to be written"),
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(gobject_class,
				    PROP_EDITABLE,
				    g_param_spec_boolean("editable",
							 _("to be written"),
							 _("to be written "),
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
    g_object_class_install_property(gobject_class,
				    PROP_SELECTION_MODE,
				    g_param_spec_enum("selection_mode",
						      _("to be written"),
						      _("to be written"),
						      GTK_TYPE_SELECTION_MODE,
						      GTK_SELECTION_SINGLE,
						      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void wordlist_view_init(WordlistView *view) {
    GtkWidget *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkListStore *store;

    GTK_WIDGET_SET_FLAGS(view, GTK_CAN_FOCUS | GTK_RECEIVES_DEFAULT);
    gtk_scrolled_window_set_policy(&view->container,
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(&view->container),
					GTK_SHADOW_IN);
    gtk_scrolled_window_set_hadjustment(&view->container, GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)));
    gtk_scrolled_window_set_vadjustment(&view->container, GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0)));

    treeview = gtk_tree_view_new();
    view->view = GTK_TREE_VIEW(treeview);

    gtk_container_add(GTK_CONTAINER(&view->container), treeview);
    gtk_widget_show(treeview);

    view->selection = gtk_tree_view_get_selection(view->view);
    gtk_tree_selection_set_mode(view->selection, GTK_SELECTION_SINGLE);


    /* Word Type */
    renderer = gtk_cell_renderer_text_new();
    g_object_set_data(G_OBJECT(renderer), "column",
		      GINT_TO_POINTER(WORDLIST_WORD_TYPE));
    column = gtk_tree_view_column_new_with_attributes(_("Word Type"), renderer,
						      "text", WORDLIST_WORD_TYPE,						      "editable", WORDLIST_EDITABLE,
						      NULL);

    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_append_column(view->view, column);
    view->word_type_column = column;

    /* Phon */
    renderer = gtk_cell_renderer_text_new();
    g_signal_connect(renderer, "edited",
		     G_CALLBACK(wordlist_view_callback_cell_edited),
		     view);
    g_object_set_data(G_OBJECT(renderer), "column",
		      GINT_TO_POINTER(WORDLIST_PHON));

    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title (column, _("Phonetic"));
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_visible(column, TRUE);
    gtk_tree_view_column_set_attributes(column, renderer,
					"text", WORDLIST_PHON,
					"editable", WORDLIST_EDITABLE,
					NULL);

/*
    column = gtk_tree_view_column_new_with_attributes("Phonetic", renderer,
						      "text", WORDLIST_PHON,
						      "editable", WORDLIST_EDITABLE,
						      NULL);
*/

    gtk_tree_view_append_column(view->view, column);
    view->phon_column = column;

    /* Desc */
    renderer = gtk_cell_renderer_text_new();
    g_signal_connect(renderer, "edited",
		     G_CALLBACK(wordlist_view_callback_cell_edited),
		     view);
    g_object_set_data(G_OBJECT(renderer), "column",
		      GINT_TO_POINTER(WORDLIST_DESC));
    column = gtk_tree_view_column_new_with_attributes("Literal", renderer,
						      "text", WORDLIST_DESC,
						      "editable", WORDLIST_EDITABLE,
						      NULL);
    gtk_tree_view_append_column(view->view, column);
    gtk_tree_view_column_set_title(column, _("Literal"));
    gtk_tree_view_column_set_visible(column, TRUE);
    view->desc_column = column;

    /* Conjection Class Code */
    renderer = gtk_cell_renderer_text_new();
    g_signal_connect(renderer, "edited",
		     G_CALLBACK(wordlist_view_callback_cell_edited),
		     view);
    g_object_set_data(G_OBJECT(renderer), "column",
		      GINT_TO_POINTER(WORDLIST_CCLASS_CODE));
    column = gtk_tree_view_column_new_with_attributes("Part of Speech", renderer,
						      "text", WORDLIST_CCLASS_CODE,
						      "editable", WORDLIST_EDITABLE,
						      NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_column_set_title(column, _("Part of Speech"));
    gtk_tree_view_append_column(view->view, column);
    view->cclass_code_column = column;

    /* Freq */
    renderer = gtk_cell_renderer_text_new();
    g_signal_connect(renderer, "edited",
		     G_CALLBACK(wordlist_view_callback_cell_edited),
		     view);
    g_object_set_data(G_OBJECT(renderer), "column",
		      GINT_TO_POINTER(WORDLIST_FREQ));
    column = gtk_tree_view_column_new_with_attributes("Frequency", renderer,
						      "text", WORDLIST_FREQ,
						      "editable", WORDLIST_EDITABLE,
						      NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_column_set_title(column, _("Frequency"));
    gtk_tree_view_append_column(view->view, column);
    view->freq_column = column;

    /* Okuri */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Okuri", renderer,
						      "text", WORDLIST_OKURI,
						      "editable", WORDLIST_EDITABLE,
						      NULL);
    gtk_tree_view_append_column(view->view, column);
    gtk_tree_view_column_set_title(column, _("Okuri"));
    gtk_tree_view_column_set_visible(column, FALSE);
    view->okuri_column = column;

    store = gtk_list_store_new(WORDLIST_N_COLUMNS,
			       G_TYPE_INT,      /* Word Type     */
			       G_TYPE_STRING,	/* Phon	         */
			       G_TYPE_STRING,	/* Desc          */
			       G_TYPE_STRING,	/* cclass code   */
			       G_TYPE_INT,	/* Frequency     */
			       G_TYPE_BOOLEAN,	/* Okuri for SKK */
			       G_TYPE_BOOLEAN); /* Editable      */
    gtk_tree_view_set_model(view->view, GTK_TREE_MODEL(store));

    view->model = GTK_TREE_MODEL(store);

    view->cclass_code_show = FALSE;
    view->freq_show = FALSE;
    view->okuri_show = FALSE;
    view->editable = FALSE;
    view->selection_mode = GTK_SELECTION_SINGLE;
}

static void wordlist_view_finalize(GObject *object) {
    WordlistView *view = WORDLIST_VIEW(object);
    if(view->model != NULL) {
	GObject *model = G_OBJECT(view->model);
	view->model = NULL;
	g_object_unref(model);
    }
    if(G_OBJECT_CLASS(parent_class)->finalize) {
	G_OBJECT_CLASS(parent_class)->finalize(object);
    }
}

static void wordlist_view_destroy(GtkObject *object) {
/*    WordlistView *view = WORDLIST_VIEW(object); */
    if(GTK_OBJECT_CLASS(parent_class)->destroy) {
	GTK_OBJECT_CLASS(parent_class)->destroy(object);
    }
}
/*
static void find_initial_path_helper(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter iter, GtkTreePath **initial_path) {
    if(*initial_path != NULL) {
	return;
    }

    *initial_path = gtk_tree_path_copy(path);
    return;
}

static GtkTreePath *wordlist_view_get_current_path(WordlistView *view) {
    GtkTreePath *path = NULL;
    GtkTreeIter iter;

    gtk_tree_view_get_cursor(view->view, &path, NULL);
    if(path != NULL) {
	return path;
    }
    gtk_widget_grab_focus(GTK_WIDGET(view));
    gtk_tree_view_get_cursor(view->view, &path, NULL);
    if(path != NULL) {
	return path;
    }

    gtk_tree_selection_selected_foreach(view->selection, find_initial_path_helper, &path);
	if(path != NULL) {
	    return path;
	}
	if(!gtk_tree_model_get_iter_first(view->model, &iter)) {
	    return NULL;
	}

	return gtk_tree_model_get_path(view->model, &iter);
}
*/

GtkWidget *wordlist_view_new(void) {
    return GTK_WIDGET(g_object_new(WORDLIST_VIEW_TYPE, NULL));
}

/**
 * wordlist_view_new_with_properties:
 * @first_property_name: name of first property to set
 * @Varargs: value of first property, followed by more properties, %NULL-terminated
 *
 * This is a function for creating a wordlist_view widget and setting
 * its properties in one go. For example you might write:
 * <literal>wordlist_view_new ("editable", TRUE, "freq_show", FALSE, NULL)
 * </literal> to create a wordlist_view with setting editable and hide
 * code column.
 *
 * Return value: a new #GtkWidget of wordlist_view.
 **/
GtkWidget *wordlist_view_new_with_attributes(const gchar *first_property_name,
					     ...)
{
    GtkWidget *widget;
    va_list args;

    va_start(args, first_property_name);
    widget = GTK_WIDGET(g_object_new_valist(WORDLIST_VIEW_TYPE,
					    first_property_name,
					    args));
    va_end(args);

    return widget;
}

void wordlist_view_set_visible_cclass_code_column(WordlistView *view,
					   gboolean show)
{
    g_return_if_fail(IS_WORDLIST_VIEW(view));
    if(view->cclass_code_show != show) {
	view->cclass_code_show = show;
	gtk_tree_view_column_set_visible(view->cclass_code_column, show);
	g_object_notify(G_OBJECT(view), "cclass_code_show");
    }
}

gboolean wordlist_view_get_visible_cclass_code_column(WordlistView *view) {
    g_return_val_if_fail(IS_WORDLIST_VIEW(view), FALSE);

    return view->cclass_code_show;
}

void wordlist_view_set_visible_freq_column(WordlistView *view, gboolean show) {
    g_return_if_fail(IS_WORDLIST_VIEW(view));
    if(view->freq_show != show) {
	view->freq_show = show;
	gtk_tree_view_column_set_visible(view->freq_column, show);
	g_object_notify(G_OBJECT(view), "freq_show");
    }
}

gboolean wordlist_view_get_visible_freq_column(WordlistView *view) {
    g_return_val_if_fail(IS_WORDLIST_VIEW(view), FALSE);

    return view->freq_show;
}

void wordlist_view_set_visible_okuri_column(WordlistView *view, gboolean show)
{
    g_return_if_fail(IS_WORDLIST_VIEW(view));
    if(view->okuri_show != show) {
	view->okuri_show = show;
	gtk_tree_view_column_set_visible(view->okuri_column, show);
	g_object_notify(G_OBJECT(view), "okuri_show");
    }
}

gboolean wordlist_view_get_visible_okuri_column(WordlistView *view) {
    g_return_val_if_fail(IS_WORDLIST_VIEW(view), FALSE);

    return view->okuri_show;
}

void wordlist_view_set_editable(WordlistView *view,
				gboolean editable)
{
    GtkTreeIter iter;
    g_return_if_fail(IS_WORDLIST_VIEW(view));

    if(view->editable != editable) {
	view->editable = editable;
	if(gtk_tree_model_get_iter_first(view->model, &iter)) {
	    do {
		gtk_list_store_set(GTK_LIST_STORE(view->model), &iter,
				   WORDLIST_EDITABLE, editable,
				   -1);
	    } while(gtk_tree_model_iter_next(view->model, &iter));

	    g_object_notify(G_OBJECT(view), "editable");
	}
    }
}

gboolean wordlist_view_get_editable(WordlistView *view) {
    g_return_val_if_fail(IS_WORDLIST_VIEW(view), FALSE);

    return view->editable;
}

void wordlist_view_set_selection_mode(WordlistView *view,
				      GtkSelectionMode mode)
{
    g_return_if_fail(IS_WORDLIST_VIEW(view));
    if(view->selection_mode != mode) {
	view->selection_mode = mode;
	gtk_tree_selection_set_mode(view->selection, mode);
	g_object_notify(G_OBJECT(view), "selection_mode");
    }
}

GtkSelectionMode wordlist_view_get_selection_mode(WordlistView *view) {
    g_return_val_if_fail(IS_WORDLIST_VIEW(view), GTK_SELECTION_SINGLE);

    return view->selection_mode;
}

static void wordlist_view_set_property(GObject *object,
				       guint prop_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
    WordlistView *view = WORDLIST_VIEW(object);

    switch(prop_id) {
    case PROP_CCLASS_CODE_SHOW:
	wordlist_view_set_visible_cclass_code_column(view,
						     g_value_get_boolean(value));
	break;
    case PROP_FREQ_SHOW:
	wordlist_view_set_visible_freq_column(view, g_value_get_boolean(value));
	break;
    case PROP_OKURI_SHOW:
	wordlist_view_set_visible_okuri_column(view, g_value_get_boolean(value));
	break;
    case PROP_EDITABLE:
	wordlist_view_set_editable(view, g_value_get_boolean(value));
	break;
    case PROP_SELECTION_MODE:
	wordlist_view_set_selection_mode(view, g_value_get_enum(value));
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

static void wordlist_view_get_property(GObject *object,
				       guint prop_id,
				       GValue *value,
				       GParamSpec *pspec)
{
    WordlistView *view = WORDLIST_VIEW(object);
    switch(prop_id) {
    case PROP_CCLASS_CODE_SHOW:
	g_value_set_boolean(value, view->cclass_code_show);
	break;
    case PROP_FREQ_SHOW:
	g_value_set_boolean(value, view->freq_show);
	break;
    case PROP_OKURI_SHOW:
	g_value_set_boolean(value, view->okuri_show);
	break;
    case PROP_EDITABLE:
	g_value_set_boolean(value, view->editable);
	break;
    case PROP_SELECTION_MODE:
	g_value_set_enum(value, view->selection_mode);
	break;
    default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	break;
    }
}

void wordlist_view_set_values(WordlistView *view,
			      word *data)
{
    GtkTreeIter iter;
    g_return_if_fail(IS_WORDLIST_VIEW(view));
    g_return_if_fail(data != NULL);

    if(data->phon == NULL)
	data->phon = g_strdup("");
    if(data->desc == NULL)
	data->desc = g_strdup("");
    if(data->cclass_code == NULL)
	data->cclass_code = g_strdup("");

    gtk_list_store_append(GTK_LIST_STORE(view->model), &iter);
    gtk_list_store_set(GTK_LIST_STORE(view->model), &iter,
		       WORDLIST_WORD_TYPE,   data->type,
		       WORDLIST_PHON,        data->phon,
		       WORDLIST_DESC,        data->desc,
		       WORDLIST_CCLASS_CODE, data->cclass_code,
		       WORDLIST_FREQ,        data->freq,
		       WORDLIST_OKURI,       data->okuri,
		       WORDLIST_EDITABLE,    view->editable,
		       -1);
}

void wordlist_view_clear(WordlistView *view) {
    g_return_if_fail(IS_WORDLIST_VIEW(view));
    g_return_if_fail(view->model);

    gtk_list_store_clear(GTK_LIST_STORE(view->model));
}

GList *wordlist_view_get_all_data_list(WordlistView *view) {
    GtkTreeIter iter;
    GList *list = NULL;
    word *data;

    g_return_val_if_fail(IS_WORDLIST_VIEW(view), NULL);

    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(view->model), &iter)) {
	do {
	    data = g_new0(word, 1);
	    if(data != NULL) {
		gtk_tree_model_get(view->model, &iter,
				   WORDLIST_WORD_TYPE,   &data->type,
				   WORDLIST_PHON,        &data->phon,
				   WORDLIST_DESC,        &data->desc,
				   WORDLIST_CCLASS_CODE, &data->cclass_code,
				   WORDLIST_FREQ,        &data->freq,
				   WORDLIST_OKURI,       &data->okuri,
				   -1);

		if(data->phon != NULL && data->desc != NULL &&
		   data->cclass_code != NULL)
		{
		    list = g_list_append(list, data);
		}
	    }
	} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(view->model), &iter));
    }
    return list;
}

GList* wordlist_view_get_selected_data_list(WordlistView *view) {
    GtkTreeIter iter;
    GList *pos, *list;
    GList *selected_data = NULL;
    g_return_val_if_fail(IS_WORDLIST_VIEW(view), NULL);

    list = gtk_tree_selection_get_selected_rows(view->selection, &view->model);

    for(pos = g_list_first(list); pos != NULL; pos = g_list_next(pos)) {
	GtkTreePath *path = pos->data;
	pos->data = gtk_tree_row_reference_new(view->model, path);
	gtk_tree_path_free(path);
    }

    for(pos = g_list_first(list); pos != NULL; pos = g_list_next(pos)) {
	GtkTreePath *path = gtk_tree_row_reference_get_path(pos->data);
	if(path) {
	    if(gtk_tree_model_get_iter(view->model, &iter, path)) {
		word *data;

		data = g_new0(word, 1);
		gtk_tree_model_get(view->model, &iter,
				   WORDLIST_WORD_TYPE,   &data->type,
				   WORDLIST_PHON,        &data->phon,
				   WORDLIST_DESC,        &data->desc,
				   WORDLIST_CCLASS_CODE, &data->cclass_code,
				   WORDLIST_FREQ,        &data->freq,
				   WORDLIST_OKURI,       &data->okuri,
				   -1);

		selected_data = g_list_append(selected_data, data);

		gtk_tree_row_reference_free(pos->data);
		gtk_tree_path_free(path);
	    }
	}
    }
    g_list_free(list);
    return selected_data;
}

void wordlist_view_remove_selected_data(WordlistView *view) {
    GtkTreeIter iter;
    GList *pos, *list;
    g_return_if_fail(IS_WORDLIST_VIEW(view));

    list = gtk_tree_selection_get_selected_rows(view->selection, &view->model);

    for(pos = g_list_first(list); pos != NULL; pos = g_list_next(pos)) {
	GtkTreePath *path = pos->data;
	pos->data = gtk_tree_row_reference_new(view->model, path);
	gtk_tree_path_free(path);
    }

    for(pos = g_list_first(list); pos != NULL; pos = g_list_next(pos)) {
	GtkTreePath *path = gtk_tree_row_reference_get_path(pos->data);
	if(path) {
	    if(gtk_tree_model_get_iter(view->model, &iter, path)) {
		gtk_list_store_remove(GTK_LIST_STORE(view->model), &iter);
		gtk_tree_row_reference_free(pos->data);
		gtk_tree_path_free(path);
	    }
	}
    }
    g_list_free(list);
    gtk_tree_selection_unselect_all(view->selection);
}

static void wordlist_view_callback_cell_edited(GtkCellRendererText *renderer,
					       const gchar *path_string,
					       const gchar *new_text,
					       WordlistView *view)
{
    GtkTreeModel *model = view->model;
    GtkTreeIter iter;
    GtkTreePath *path;
    gchar *old_text;
    gint *column;

    path = gtk_tree_path_new_from_string(path_string);
    column = g_object_get_data(G_OBJECT(renderer), "column");

    gtk_tree_model_get_iter(model, &iter, path);

    switch(GPOINTER_TO_INT(column)) {
    case WORDLIST_PHON:
    case WORDLIST_DESC:
    case WORDLIST_CCLASS_CODE:
	gtk_tree_model_get(model, &iter,
			   column, &old_text,
			   -1);
	g_free(old_text);
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			   column, new_text,
			   -1);
	break;
    case WORDLIST_FREQ:
	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			   column, atoi(new_text),
			   -1);
	break;
    default:
	break;
    }
    gtk_tree_path_free(path);
}

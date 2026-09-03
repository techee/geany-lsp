/*
 * Copyright 2026 Jiri Techet <techet@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "lsp-popup.h"
#include "lsp-diagnostics.h"
#include "lsp-hover.h"
#include "lsp-server.h"
#include "lsp-utils.h"

#include <gdk/gdkkeysyms.h>


/* delay after which the mouse is considered resting at its position and
 * hover or diagnostic popups get shown */
#define DWELL_TIME 500

/* space between the label and the popup window edges */
#define POPUP_PADDING 5


extern GeanyPlugin *geany_plugin;
extern GeanyData *geany_data;


typedef enum
{
	/* hover and diagnostic popups - shown and hidden based on the mouse
	 * position within the editor */
	PopupKindHover,
	/* signature popups - keyboard-driven, surviving typing and unaffected
	 * by mouse movements */
	PopupKindSignature,
} PopupKind;


static GtkWidget *popup_window;
static GtkWidget *popup_scrollwin;
static GtkWidget *popup_label;
static ScintillaObject *popup_sci;
static PopupKind popup_kind;
static GtkCssProvider *popup_css_provider;
static gchar *popup_css;
static gulong popup_sci_focus_handler;
static gulong popup_sci_button_handler;
static gulong popup_sci_destroy_handler;
static guint popup_key_snooper;

static guint dwell_timer_source;
static gint dwell_x;
static gint dwell_y;
static GeanyDocument *dwell_doc;


static gboolean is_inside_popup(gint x_root, gint y_root)
{
	gint win_x, win_y;

	if (!popup_window || !gtk_widget_get_visible(popup_window))
		return FALSE;

	gtk_window_get_position(GTK_WINDOW(popup_window), &win_x, &win_y);

	return x_root >= win_x && x_root < win_x + gtk_widget_get_allocated_width(popup_window) &&
		y_root >= win_y && y_root < win_y + gtk_widget_get_allocated_height(popup_window);
}


static gboolean showing_signature(ScintillaObject *sci)
{
	return popup_kind == PopupKindSignature && popup_sci == sci &&
		popup_window && gtk_widget_get_visible(popup_window);
}


/* the bold range highlights the active parameter of signature popups; the
 * offsets are in bytes */
static PangoAttrList *create_bold_attrs(gint bold_start, gint bold_end)
{
	PangoAttrList *attrs = pango_attr_list_new();

	if (bold_start >= 0 && bold_end > bold_start)
	{
		PangoAttribute *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);

		attr->start_index = bold_start;
		attr->end_index = bold_end;
		pango_attr_list_insert(attrs, attr);
	}

	return attrs;
}


static void hide_popup_window(void)
{
	if (popup_key_snooper != 0)
	{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_key_snooper_remove(popup_key_snooper);
G_GNUC_END_IGNORE_DEPRECATIONS
		popup_key_snooper = 0;
	}

	if (popup_sci)
	{
		g_signal_handler_disconnect(popup_sci, popup_sci_focus_handler);
		g_signal_handler_disconnect(popup_sci, popup_sci_button_handler);
		g_signal_handler_disconnect(popup_sci, popup_sci_destroy_handler);
		popup_sci = NULL;
	}

	if (popup_window)
		gtk_widget_hide(popup_window);
}


/* Keys are handled using a key snooper running before any other key handling.
 * This is necessary because the popup window never gets keyboard focus so key
 * presses are delivered to the main Geany window where e.g. Ctrl+C gets
 * consumed by the Geany "Copy" keybinding before it could ever reach any
 * handler connected to the editor widget */
static gint key_snooper(GtkWidget *grab_widget, GdkEventKey *event, gpointer user_data)
{
	guint mods = event->state & gtk_accelerator_get_default_mod_mask();

	if (event->type != GDK_KEY_PRESS)
		return FALSE;

	if (event->keyval == GDK_KEY_Escape)
	{
		/* when the autocompletion list is shown, let Scintilla cancel it
		 * first - the popup gets closed by the next Escape */
		if (popup_sci && SSM(popup_sci, SCI_AUTOCACTIVE, 0, 0))
			return FALSE;

		hide_popup_window();
		return TRUE;
	}

	/* redirect Ctrl+C (Cmd+C on macOS) to the popup label when it has
	 * selected text */
	if (mods == GEANY_PRIMARY_MOD_MASK &&
		(event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_C ||
		 event->keyval == GDK_KEY_Insert || event->keyval == GDK_KEY_KP_Insert))
	{
		gint start, end;

		if (gtk_label_get_selection_bounds(GTK_LABEL(popup_label), &start, &end))
		{
			g_signal_emit_by_name(popup_label, "copy-clipboard");
			return TRUE;
		}
	}

	return FALSE;
}


static gboolean on_sci_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	hide_popup_window();
	return FALSE;
}


static gboolean on_sci_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	hide_popup_window();
	return FALSE;
}


static void on_sci_destroy(GtkWidget *widget, gpointer user_data)
{
	hide_popup_window();
}


static void create_popup_window(void)
{
	GtkWidget *frame;

	popup_window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_set_name(popup_window, "lsp-popup");
	gtk_window_set_transient_for(GTK_WINDOW(popup_window), GTK_WINDOW(geany->main_widgets->window));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(popup_window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(popup_window), GDK_WINDOW_TYPE_HINT_TOOLTIP);
	gtk_window_set_resizable(GTK_WINDOW(popup_window), FALSE);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(popup_window), frame);

	popup_scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(popup_scrollwin),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(frame), popup_scrollwin);

	popup_label = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(popup_label), 0.0);
	gtk_label_set_yalign(GTK_LABEL(popup_label), 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(popup_label), TRUE);
	gtk_label_set_line_wrap_mode(GTK_LABEL(popup_label), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_selectable(GTK_LABEL(popup_label), TRUE);
	/* prevent GTK from focusing the label when the window is shown for the
	 * first time, which would select the whole label text (mouse selection
	 * inside the label works even without focus) */
	gtk_widget_set_can_focus(popup_label, FALSE);
	gtk_widget_set_margin_start(popup_label, POPUP_PADDING);
	gtk_widget_set_margin_end(popup_label, POPUP_PADDING);
	gtk_widget_set_margin_top(popup_label, POPUP_PADDING);
	gtk_widget_set_margin_bottom(popup_label, POPUP_PADDING);
	gtk_container_add(GTK_CONTAINER(popup_scrollwin), popup_label);

	/* show the child widgets right away - hidden widgets have zero size so
	 * measuring the window for positioning would return a bogus size before
	 * the popup is shown for the first time */
	gtk_widget_show_all(frame);

	popup_css_provider = gtk_css_provider_new();
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
		GTK_STYLE_PROVIDER(popup_css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}


static void append_color_css(GString *css, const gchar *selector, const gchar *property,
	const gchar *color)
{
	GdkRGBA rgba;

	if (!EMPTY(color) && gdk_rgba_parse(&rgba, color))
		g_string_append_printf(css, "%s { %s: %s; } ", selector, property, color);
}


/* applies the configured popup colors; colors without configured values are
 * not styled and keep the values from the GTK theme */
static void update_popup_colors(LspServerConfig *cfg)
{
	GString *css = g_string_new(NULL);

	append_color_css(css, "#lsp-popup", "background-color",
		cfg->popup_background_color);
	append_color_css(css, "#lsp-popup frame > border", "border-color",
		cfg->popup_border_color);
	append_color_css(css, "#lsp-popup label", "color",
		cfg->popup_text_color);

	if (g_strcmp0(css->str, popup_css) != 0)
	{
		SETPTR(popup_css, g_string_free(css, FALSE));
		gtk_css_provider_load_from_data(popup_css_provider, popup_css, -1, NULL);
	}
	else
		g_string_free(css, TRUE);
}


static void position_popup_window(ScintillaObject *sci, gint pos, PopupKind kind)
{
	GtkRequisition win_size;
	GdkMonitor *monitor;
	gboolean show_above = kind == PopupKindSignature;
	gint sci_x, sci_y, origin_x, origin_y, win_x, win_y;
	gint sci_line_height;

	/* Signature popups are shown above the edited text so they don't obscure
	 * it or the autocompletion popup shown below it, flipped below the text
	 * only when there isn't enough space above. The other popups are shown
	 * below their position, flipped above it when there isn't enough space
	 * below */
	sci_x = SSM(sci, SCI_POINTXFROMPOSITION, 0, pos);
	sci_y = SSM(sci, SCI_POINTYFROMPOSITION, 0, pos);
	sci_line_height = SSM(sci, SCI_TEXTHEIGHT, SSM(sci, SCI_LINEFROMPOSITION, pos, 0), 0);
	gdk_window_get_origin(gtk_widget_get_window(GTK_WIDGET(sci)), &origin_x, &origin_y);

	gtk_widget_get_preferred_size(popup_window, NULL, &win_size);

	monitor = gdk_display_get_monitor_at_point(gtk_widget_get_display(GTK_WIDGET(sci)),
		origin_x + sci_x, origin_y + sci_y);

	win_x = origin_x + sci_x;
	if (show_above)
		win_y = origin_y + sci_y - win_size.height;
	else
		win_y = origin_y + sci_y + sci_line_height;

	if (monitor)
	{
		GdkRectangle workarea;

		gdk_monitor_get_workarea(monitor, &workarea);

		/* flip to the other side of the text when there isn't enough space
		 * at the preferred side */
		if (!show_above && win_y + win_size.height > workarea.y + workarea.height)
			win_y = origin_y + sci_y - win_size.height;
		else if (show_above && win_y < workarea.y)
			win_y = origin_y + sci_y + sci_line_height;

		if (win_y + win_size.height > workarea.y + workarea.height)
			win_y = workarea.y + workarea.height - win_size.height;
		if (win_y < workarea.y)
			win_y = workarea.y;
		if (win_x + win_size.width > workarea.x + workarea.width)
			win_x = workarea.x + workarea.width - win_size.width;
		if (win_x < workarea.x)
			win_x = workarea.x;
	}

	gtk_window_move(GTK_WINDOW(popup_window), win_x, win_y);
}


static void show_popup(GeanyDocument *doc, gint pos, const gchar *text, PopupKind kind,
	gint bold_start, gint bold_end)
{
	LspServer *srv = lsp_server_get_if_running(doc);
	ScintillaObject *sci = doc->editor->sci;
	PangoFontMetrics *metrics;
	PangoAttrList *attrs;
	PangoLayout *layout;
	gint line_height, char_width, max_width, max_height;
	gint content_width, content_height, width, height;

	if (!srv)
		return;

	attrs = create_bold_attrs(bold_start, bold_end);

	/* when the popup already shows the same text, keep it where it is instead
	 * of repositioning it (e.g. when hovering over a different position of
	 * the same symbol) */
	if (popup_window && gtk_widget_get_visible(popup_window) && popup_sci == sci &&
		popup_kind == kind &&
		g_strcmp0(gtk_label_get_text(GTK_LABEL(popup_label)), text) == 0)
	{
		/* the highlighted parameter may still have changed */
		gtk_label_set_attributes(GTK_LABEL(popup_label), attrs);
		pango_attr_list_unref(attrs);
		return;
	}

	if (!popup_window)
		create_popup_window();

	hide_popup_window();

	update_popup_colors(&srv->config);
	ui_widget_modify_font_from_string(popup_label, geany_data->interface_prefs->editor_font);
	gtk_label_set_text(GTK_LABEL(popup_label), text);
	gtk_label_set_attributes(GTK_LABEL(popup_label), attrs);

	metrics = pango_context_get_metrics(gtk_widget_get_pango_context(popup_label), NULL, NULL);
	line_height = PANGO_PIXELS(pango_font_metrics_get_ascent(metrics) +
		pango_font_metrics_get_descent(metrics));
	char_width = PANGO_PIXELS(pango_font_metrics_get_approximate_char_width(metrics));
	pango_font_metrics_unref(metrics);

	max_width = MAX(srv->config.popup_max_width, 10) * char_width;
	max_height = MAX(srv->config.popup_max_lines, 1) * line_height;

	/* measure the size of the displayed text so the window can shrink for
	 * short messages instead of always using the maximum size */
	layout = gtk_widget_create_pango_layout(popup_label, text);
	pango_layout_set_attributes(layout, attrs);
	pango_attr_list_unref(attrs);
	pango_layout_get_pixel_size(layout, &content_width, &content_height);
	if (content_width > max_width)
	{
		pango_layout_set_width(layout, max_width * PANGO_SCALE);
		pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
		pango_layout_get_pixel_size(layout, &content_width, &content_height);
		content_width = max_width;
	}
	g_object_unref(layout);

	width = content_width + 2 * POPUP_PADDING;
	height = MIN(content_height, max_height) + 2 * POPUP_PADDING;
	if (content_height > max_height)
	{
		GtkWidget *scrollbar = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(popup_scrollwin));
		gint scrollbar_width = 0;

		if (scrollbar)
			gtk_widget_get_preferred_width(scrollbar, NULL, &scrollbar_width);
		width += scrollbar_width;
	}

	/* Enable the vertical scrollbar only when the whole message doesn't fit
	 * the popup. With GTK_POLICY_AUTOMATIC, the minimum height of the
	 * scrolled window is the minimum height of the scrollbar (even when not
	 * shown), making popups with a single line of text unnecessarily tall */
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(popup_scrollwin), GTK_POLICY_NEVER,
		content_height > max_height ? GTK_POLICY_AUTOMATIC : GTK_POLICY_NEVER);
	gtk_widget_set_size_request(popup_scrollwin, width, height);
	gtk_window_resize(GTK_WINDOW(popup_window), 1, 1);

	position_popup_window(sci, pos, kind);
	gtk_widget_show(popup_window);

	gtk_adjustment_set_value(
		gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(popup_scrollwin)), 0);

	popup_sci = sci;
	popup_kind = kind;
	popup_sci_focus_handler = g_signal_connect(sci, "focus-out-event",
		G_CALLBACK(on_sci_focus_out), NULL);
	popup_sci_button_handler = g_signal_connect(sci, "button-press-event",
		G_CALLBACK(on_sci_button_press), NULL);
	popup_sci_destroy_handler = g_signal_connect(sci, "destroy",
		G_CALLBACK(on_sci_destroy), NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	popup_key_snooper = gtk_key_snooper_install(key_snooper, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
}


void lsp_popup_show(GeanyDocument *doc, gint pos, const gchar *text)
{
	show_popup(doc, pos, text, PopupKindHover, -1, -1);
}


void lsp_popup_show_signature(GeanyDocument *doc, gint pos, const gchar *text,
	gint bold_start, gint bold_end)
{
	show_popup(doc, pos, text, PopupKindSignature, bold_start, bold_end);
}


gboolean lsp_popup_showing_signature(GeanyDocument *doc)
{
	return showing_signature(doc->editor->sci);
}


void lsp_popup_hide(GeanyDocument *doc)
{
	if (doc->editor->sci == popup_sci)
		hide_popup_window();
}


static void cancel_dwell_timer(void)
{
	if (dwell_timer_source != 0)
	{
		g_source_remove(dwell_timer_source);
		dwell_timer_source = 0;
	}
}


static gboolean on_dwell_timer_expired(gpointer user_data)
{
	GeanyDocument *doc = dwell_doc;
	ScintillaObject *sci;
	LspServer *srv;
	gint pos;

	dwell_timer_source = 0;

	if (!DOC_VALID(doc))
		return G_SOURCE_REMOVE;

	sci = doc->editor->sci;
	srv = lsp_server_get_if_running(doc);

	if (!srv || !gtk_widget_has_focus(GTK_WIDGET(sci)))
		return G_SOURCE_REMOVE;

	/* signature popups are keyboard-driven - don't replace or hide them
	 * based on the mouse position */
	if (showing_signature(sci))
		return G_SOURCE_REMOVE;

	pos = SSM(sci, SCI_POSITIONFROMPOINTCLOSE, dwell_x, dwell_y);
	if (pos < 0)
	{
		/* not over any text - nothing to display for this position */
		lsp_popup_hide(doc);
		return G_SOURCE_REMOVE;
	}

	if (srv->config.diagnostics_enable && lsp_diagnostics_has_diag(pos))
		lsp_diagnostics_show_popup(pos);
	else if (srv->config.hover_enable && srv->config.hover_available)
		lsp_hover_send_request(srv, doc, pos, FALSE);
	else
		lsp_popup_hide(doc);

	return G_SOURCE_REMOVE;
}


static gboolean on_sci_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
	GeanyDocument *doc = user_data;

	/* ignore events from other windows of the Scintilla widget */
	if (event->window != gtk_widget_get_window(widget))
		return FALSE;

	/* restart the timer measuring how long the mouse rests at its position */
	cancel_dwell_timer();
	dwell_doc = doc;
	dwell_x = (gint)event->x;
	dwell_y = (gint)event->y;
	dwell_timer_source = plugin_timeout_add(geany_plugin, DWELL_TIME,
		on_dwell_timer_expired, NULL);

	return FALSE;
}


static gboolean on_sci_leave_notify(GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
	GeanyDocument *doc = user_data;

	cancel_dwell_timer();

	/* hide the popup when the mouse leaves the editor area, except when it
	 * moves inside the popup itself or when a keyboard-driven signature
	 * popup is shown */
	if (!is_inside_popup((gint)event->x_root, (gint)event->y_root) &&
		!showing_signature(doc->editor->sci))
		lsp_popup_hide(doc);

	return FALSE;
}


void lsp_popup_mouse_tracking_init(GeanyDocument *doc)
{
	ScintillaObject *sci = doc->editor->sci;

	if (!g_object_get_data(G_OBJECT(sci), "lsp-mouse-tracking"))
	{
		g_object_set_data(G_OBJECT(sci), "lsp-mouse-tracking", GINT_TO_POINTER(TRUE));
		plugin_signal_connect(geany_plugin, G_OBJECT(sci), "motion-notify-event", FALSE,
			G_CALLBACK(on_sci_motion_notify), doc);
		plugin_signal_connect(geany_plugin, G_OBJECT(sci), "leave-notify-event", FALSE,
			G_CALLBACK(on_sci_leave_notify), doc);
	}
}


void lsp_popup_destroy(void)
{
	cancel_dwell_timer();
	hide_popup_window();
	if (popup_window)
	{
		gtk_widget_destroy(popup_window);
		popup_window = NULL;
		popup_scrollwin = NULL;
		popup_label = NULL;
	}
	if (popup_css_provider)
	{
		gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(),
			GTK_STYLE_PROVIDER(popup_css_provider));
		g_object_unref(popup_css_provider);
		popup_css_provider = NULL;
	}
	g_free(popup_css);
	popup_css = NULL;
}

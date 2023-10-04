/*
 * Copyright 2023 Jiri Techet <techet@gmail.com>
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

#include "lsp/lsp-log.h"
#include "lsp/lsp-utils.h"

#include <glib.h>


LspLogInfo lsp_log_start(LspServerConfig *config)
{
	LspLogInfo info = {0, NULL};
	GFile *fp;

	if (!config->rpc_log)
		return info;

	if (g_strcmp0(config->rpc_log, "stdout") == 0)
	{
		info.type = STDOUT_FILENO;
		return info;
	}
	else if (g_strcmp0(config->rpc_log, "stderr") == 0)
	{
		info.type = STDERR_FILENO;
		return info;
	}

	fp = g_file_new_for_path(config->rpc_log);
	g_file_delete(fp, NULL, NULL);
	info.stream = g_file_create(fp, G_FILE_CREATE_NONE, NULL, NULL);

	if (!info.stream)
		g_warning("failed to create log file: %s\n", config->rpc_log);

	g_object_unref(fp);

	return info;
}


void lsp_log_stop(LspLogInfo log)
{
	if (!log.stream)
		return;

	g_output_stream_close(G_OUTPUT_STREAM(log.stream), NULL, NULL);
}


void lsp_log(LspLogInfo log, LspLogType type, const gchar *method, GVariant *params)
{
	gchar *json_msg;
	const gchar *title = "";
	GDateTime *time;
	gchar *time_str;
	const gchar *fmt_str = "%s %s: %s\n\n%s\n\n";

	if (log.type == 0 && !log.stream)
		return;

	time = g_date_time_new_now_local();
	time_str = g_date_time_format(time, "\%H:\%M:\%S.\%f");
	g_date_time_unref(time);

	if (!method)
		method = "";

	switch (type)
	{
		case LspLogClientMessageSent:
			title = "Geany ---> Server\nmessage request";
			break;
		case LspLogClientMessageReceived:
			title = "Geany <--- Server\nmessage response";
			break;
		case LspLogClientNotificationSent:
			title = "Geany ---> Server\nnotification";
			break;
		case LspLogServerMessageSent:
			title = "Server ---> Geany\nmessage request";
			break;
		case LspLogServerMessageReceived:
			title = "Server <--- Geany\nmessage response";
			break;
		case LspLogServerNotificationSent:
			title = "Server ---> Geany\nnotification";
			break;
	}

	json_msg = lsp_utils_json_pretty_print(params);

	if (log.type == STDOUT_FILENO)
		printf(fmt_str, time_str, title, method, json_msg);
	else if (log.type == STDERR_FILENO)
		fprintf(stderr, fmt_str, time_str, title, method, json_msg);
	else
		g_output_stream_printf(G_OUTPUT_STREAM(log.stream), NULL, NULL, NULL,
			fmt_str, time_str, title, method, json_msg);

	g_free(json_msg);
	g_free(time_str);
}

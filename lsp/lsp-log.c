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


static void log_print(LspLogInfo log, const gchar *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	if (log.type == STDOUT_FILENO)
		vprintf(fmt, args);
	else if (log.type == STDERR_FILENO)
		vfprintf(stderr, fmt, args);
	else
		g_output_stream_vprintf(G_OUTPUT_STREAM(log.stream), NULL, NULL, NULL, fmt, args);

	va_end(args);
}


LspLogInfo lsp_log_start(LspServerConfig *config)
{
	LspLogInfo info = {0, TRUE, NULL};
	GFile *fp;

	if (!config->rpc_log)
		return info;

	info.full = config->rpc_log_full;

	if (g_strcmp0(config->rpc_log, "stdout") == 0)
		info.type = STDOUT_FILENO;
	else if (g_strcmp0(config->rpc_log, "stderr") == 0)
		info.type = STDERR_FILENO;
	else
	{
		fp = g_file_new_for_path(config->rpc_log);
		g_file_delete(fp, NULL, NULL);
		info.stream = g_file_create(fp, G_FILE_CREATE_NONE, NULL, NULL);

		if (!info.stream)
			g_warning("failed to create log file: %s\n", config->rpc_log);

		g_object_unref(fp);
	}

	if (info.full)
		log_print(info, "{\n");

	return info;
}


void lsp_log_stop(LspLogInfo log)
{
	if (log.type == 0 && !log.stream)
		return;

	if (log.full)
		log_print(log, "\n\n\"log end\": \"\"\n}\n");

	if (log.stream)
		g_output_stream_close(G_OUTPUT_STREAM(log.stream), NULL, NULL);
}


void lsp_log(LspLogInfo log, LspLogType type, const gchar *method, GVariant *params)
{
	gchar *json_msg, *time_str;
	const gchar *title = "";
	GDateTime *time;

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
			title = "Geany  ---> Server (message request)";
			break;
		case LspLogClientMessageReceived:
			title = "Geany  <--- Server (message response)";
			break;
		case LspLogClientNotificationSent:
			title = "Geany  ---> Server (notification)";
			break;
		case LspLogServerMessageSent:
			title = "Server ---> Geany  (message request)";
			break;
		case LspLogServerMessageReceived:
			title = "Server <--- Geany  (message response)";
			break;
		case LspLogServerNotificationSent:
			title = "Server ---> Geany  (notification)";
			break;
	}

	if (log.full)
	{
		log_print(log, "\n\n\"%s\": \"%s\",\n", time_str, title);

		if (!params)
			json_msg = g_strdup("null");
		else
			json_msg = lsp_utils_json_pretty_print(params);

		log_print(log, "\"%s\":\n%s,\n", method, json_msg);
		g_free(json_msg);
	}
	else
		log_print(log, "[%s] %s\n                  - %s\n", time_str, title, method);

	g_free(time_str);
}

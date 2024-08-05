project(
  'geany-lsp',
  'c',
  license : 'GPL-2.0',
  version : '2.1'
)

plugin_name = 'lsp'

i18n = import('i18n')
# define GETTEXT_PACKAGE
add_project_arguments('-DGETTEXT_PACKAGE="intltest"', language:'c')
i18n.gettext(meson.project_name())

subdir('meson_config')


geany_dep = dependency('geany')

plugin_suffix = []
if (host_machine.system() == 'darwin')
	plugin_suffix = 'so'  # use "so" instead of "dylib" so Geany finds plugins correctly
endif

plugin_inc = [include_directories('lsp/deps/json-glib', 'lsp/deps/jsonrpc-glib', 'lsp/deps', 'lsp/src')]

shared_module('lsp',
	'lsp/deps/json-glib/json-array.c',
	'lsp/deps/json-glib/json-builder.c',
	'lsp/deps/json-glib/json-debug.c',
	'lsp/deps/json-glib/json-gboxed.c',
	'lsp/deps/json-glib/json-generator.c',
	'lsp/deps/json-glib/json-gobject.c',
	'lsp/deps/json-glib/json-gvariant.c',
	'lsp/deps/json-glib/json-node.c',
	'lsp/deps/json-glib/json-object.c',
	'lsp/deps/json-glib/json-parser.c',
	'lsp/deps/json-glib/json-path.c',
	'lsp/deps/json-glib/json-reader.c',
	'lsp/deps/json-glib/json-scanner.c',
	'lsp/deps/json-glib/json-serializable.c',
	'lsp/deps/json-glib/json-utils.c',
	'lsp/deps/json-glib/json-value.c',

	'lsp/deps/jsonrpc-glib/jsonrpc-client.c',
	'lsp/deps/jsonrpc-glib/jsonrpc-input-stream.c',
	'lsp/deps/jsonrpc-glib/jsonrpc-message.c',
	'lsp/deps/jsonrpc-glib/jsonrpc-output-stream.c',
	'lsp/deps/jsonrpc-glib/jsonrpc-server.c',
	'lsp/deps/jsonrpc-glib/jsonrpc-marshalers.c',

	'lsp/src/lsp-autocomplete.c',
	'lsp/src/lsp-main.c',
	'lsp/src/lsp-server.c',
	'lsp/src/lsp-sync.c',
	'lsp/src/lsp-rpc.c',
	'lsp/src/lsp-diagnostics.c',
	'lsp/src/lsp-hover.c',
	'lsp/src/lsp-signature.c',
	'lsp/src/lsp-log.c',
	'lsp/src/lsp-goto.c',
	'lsp/src/lsp-progress.c',
	'lsp/src/lsp-symbols.c',
	'lsp/src/lsp-symbol-kinds.c',
	'lsp/src/lsp-semtokens.c',
	'lsp/src/lsp-goto-panel.c',
	'lsp/src/lsp-goto-anywhere.c',
	'lsp/src/lsp-format.c',
	'lsp/src/lsp-highlight.c',
	'lsp/src/lsp-rename.c',
	'lsp/src/lsp-command.c',
	'lsp/src/lsp-code-lens.c',
	'lsp/src/lsp-symbol.c',
	'lsp/src/lsp-extension.c',
	'lsp/src/lsp-utils.c',
	'lsp/src/lsp-workspace-folders.c',
	name_prefix: '',  # "lib" seems to be the default prefix
	name_suffix: plugin_suffix,
	include_directories: plugin_inc,
	c_args: ['-DJSON_COMPILATION', '-DJSONRPC_GLIB_COMPILATION'],
	dependencies: [config_dep, geany_dep],
	install_dir: plugin_path,
	install: true
)


lsp_conf = [
	'lsp/data/lsp.conf',
]
install_data(sources: lsp_conf, install_dir: plugin_datadir)

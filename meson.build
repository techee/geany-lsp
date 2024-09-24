project(
  'geany-lsp',
  'c',
  license : 'GPL-2.0',
  version : '2.1'
)

plugin_name = 'lsp'


geany_dep = dependency('geany', version : '>=2.1')


i18n = import('i18n')
# define GETTEXT_PACKAGE
add_project_arguments('-DGETTEXT_PACKAGE="intltest"', language:'c')
i18n.gettext(meson.project_name())


docdir = get_option('datadir') / 'doc'
project_datadir = get_option('datadir') / 'geany-plugins'
project_libdir = get_option('libdir') / 'geany-plugins'
project_docdir = docdir / 'geany-plugins'

plugin_datadir = project_datadir / plugin_name
plugin_libdir = project_libdir / plugin_name
plugin_docdir = project_docdir / plugin_name

plugin_path = get_option('libdir') / 'geany'

conf_data = configuration_data()

conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set_quoted('GEANY_VERSION', '2.0.0')
conf_data.set_quoted('PLUGIN', plugin_name)
conf_data.set_quoted('DOCDIR', get_option('prefix') / docdir)
conf_data.set_quoted('LIBDIR', get_option('prefix') / get_option('libdir'))
conf_data.set_quoted('LOCALEDIR', get_option('prefix') / get_option('localedir'))
conf_data.set_quoted('GEANYPLUGINS_DATADIR', get_option('prefix') / plugin_datadir)
conf_data.set_quoted('DBGPLUG_DATA_DIR', get_option('prefix') / plugin_datadir)
conf_data.set_quoted('PLUGINDATADIR', get_option('prefix') / plugin_datadir)
conf_data.set_quoted('PLUGINDOCDIR', get_option('prefix') / plugin_docdir)
conf_data.set_quoted('PLUGINHTMLDOCDIR', get_option('prefix') / plugin_docdir / 'html')

configure_file(
  output: 'config.h',
  configuration: conf_data,
)


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
	'lsp/src/lsp-selection-range.c',
	'lsp/src/lsp-symbol.c',
	'lsp/src/lsp-symbols.c',
	'lsp/src/lsp-symbol-kinds.c',
	'lsp/src/lsp-symbol-tree.c',
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
	c_args: ['-DHAVE_CONFIG_H', '-DJSON_COMPILATION', '-DJSONRPC_GLIB_COMPILATION'],
	dependencies: [geany_dep],
	install_dir: plugin_path,
	install: true
)


lsp_conf = [
	'lsp/data/lsp.conf',
]
install_data(sources: lsp_conf, install_dir: plugin_datadir)

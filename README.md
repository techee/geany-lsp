geany-lsp
=========

This is the development repository of the LSP plugin for
[Geany](https://github.com/geany/geany).

Main development of the plugin happens in this repository so please report
bugs here. In addition, the plugin code will be uploaded to the
[geany-plugins](https://github.com/geany/geany-plugins) before every release
so if you are interested in the release version only, you can get it from
there.

Features
--------

The plugin provides the following LSP features:

* Autocompletion
* Function signagure
* Diagnostic messages
* Code lenses
* Semantic token type highlighting
* Hover popup
* Symbol tree
* Go to symbol definition/declaration
* Go to type definition
* Swap header/source
* Find references
* Find implementations
* Navigation to document/project symbols, files, and line numbers
* Code formatting
* Identical symbol highlighting
* Document symbol renaming
* Project-wide renaming

Limitations
-----------

By design, the plugin communicates over stdin/stdout only, is responsible
for launching and terminating the language server process, and supports only
a single language server per programming language.

All of these limitations are addressed by the [LSP proxy](https://github.com/techee/lsp-proxy)
project and related issues should be directed there.

Building
--------

For building the plugin, you need to compile and install the latest Geany
version (2.1) from git master. Then, you can build and install the LSP plugin
using either
```
./autogen.sh && make && sudo make install
```
or
```
meson setup build && cd build && ninja && sudo ninja install
```

Quick Start
-----------

1. Install the LSP server you want to use.
2. Enable the LSP Client plugin under `Tools->Plugin Manager`.
3. Check the configuration file under `Tools->LSP Client->User Configuration`
and make sure the LSP server is configured correctly, following the documentation
of the LSP server you wish to use.
4. Create a Geany project and set its base path to the root directory of
your sources.
5. Enable the LSP plugin for the project under `Project->Properties->LSP Client`.

Documentation
-------------

More information about the supported features and configuration can be found in
* [the plugin documentation](https://github.com/techee/geany-lsp/tree/master/lsp/README)
* [the configuration file](https://github.com/techee/geany-lsp/blob/master/lsp/data/lsp.conf)

---

Jiri Techet, 2024

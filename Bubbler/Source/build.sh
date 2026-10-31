#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 plugin/make_tables.py
python3 plugin/make_art.py
python3 plugin-kit/plugin_tool.py build plugin --out build/bubbler.opmod
python3 plugin-kit/plugin_tool.py show build/bubbler.opmod

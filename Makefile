.SILENT:

.PHONY: docs docs-server docs-watch docs-sync-server

_invalid:
	echo "Specify a target name to execute"

docs:
	sphinx-build -b html \
		docs \
		_build/docs \
		-Wqaj8
	echo "Docs generated to _build/docs"

docs-server: docs
	echo "Docs are visible on http://localhost:9794/"
	cd _build/docs && \
		python -m http.server 9794

docs-watch: docs
	+sh tools/docs-watch.sh

docs-sync-server: docs
	cd _build/docs && \
	browser-sync start --server \
		--reload-delay 300 \
		--watch **/*.html

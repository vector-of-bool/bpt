.SILENT:

.PHONY: \
	docs docs-server docs-watch docs-sync-server nix-ci linux-ci macos-ci \
	vagrant-freebsd-ci site alpine-static-ci

_invalid:
	echo "Specify a target name to execute"
	exit 1

clean:
	rm -f -r -- $(shell find -name __pycache__ -type d)
	rm -f -r -- _build/ _prebuilt/

docs:
	sphinx-build -b html \
		docs \
		_build/docs \
		-d _build/doctrees \
		-Wqanj8
	echo "Docs generated to _build/docs"

hugo-docs:
	env GEN_FOR_HUGO=1 $(MAKE) docs

docs-server: docs
	echo "Docs are visible on http://localhost:9794/"
	cd _build/docs && \
		python -m http.server 9794

docs-watch: docs
	+sh tools/docs-watch.sh

docs-sync-server:
	mkdir -p _build/docs
	cd _build/docs && \
	browser-sync start --server \
		--reload-delay 300 \
		--watch **/*.html

macos-ci:
	python3 -u tools/ci.py \
		-B download \
		-T tools/gcc-9-rel-macos.jsonc
	mv _build/dds _build/dds-macos-x64

linux-ci:
	python3 -u tools/ci.py \
		-B download \
		-T tools/gcc-9-static-rel.jsonc
	mv _build/dds _build/dds-linux-x64

nix-ci:
	python3 -u tools/ci.py \
		-B download \
		-T tools/gcc-9-rel.jsonc

alpine-static-ci:
	docker build -t dds-builder -f tools/Dockerfile.alpine tools/
	docker run \
		-ti --rm \
		-u $(shell id -u) \
		-v $(PWD):/host -w /host \
		--privileged \
		dds-builder \
		make linux-ci

vagrant-freebsd-ci:
	vagrant up freebsd11
	vagrant rsync
	vagrant ssh freebsd11 -c '\
		cd /vagrant && \
		python3.7 tools/ci.py \
			-B download \
			-T tools/freebsd-gcc-10.jsonc \
		'
	mkdir -p _build/
	vagrant scp freebsd11:/vagrant/_build/dds _build/dds-freebsd-x64
	vagrant halt

site: docs
	rm -r -f -- _site/
	mkdir -p _site/
	cp site/index.html _site/
	cp -r _build/docs _site/
	echo "Site generated at _site/"

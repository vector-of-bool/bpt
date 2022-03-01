.SILENT:

.PHONY: \
	docs docs-server docs-watch docs-sync-server linux-ci macos-ci \
	vagrant-freebsd-ci site alpine-static-ci _alpine-static-ci poetry-setup \
	full-ci dev-build release-build

docs:
	poetry run dagon docs
	echo "Docs generated to _build/docs"

hugo-docs:
	env GEN_FOR_HUGO=1 $(MAKE) docs

docs-server: docs
	echo "Docs are visible on http://localhost:9794/"
	cd _build/docs && \
		python -m http.server 9794

docs-watch: docs
	+poetry run sh tools/docs-watch.sh

docs-sync-server:
	mkdir -p _build/docs
	cd _build/docs && \
	browser-sync start --server \
		--reload-delay 300 \
		--watch **/*.html

.poetry.stamp: poetry.lock
	poetry install --no-dev
	touch .poetry.stamp

poetry-setup: .poetry.stamp

full-ci: poetry-setup
	poetry run dagon clean ci

dev-build: poetry-setup
	poetry run dagon clean build.test

release-build: poetry-setup
	poetry run dagon clean build.main

macos-ci: full-ci
	mv _build/dds _build/dds-macos-x64

linux-ci: full-ci
	mv _build/dds _build/dds-linux-x64

_alpine-static-ci:
	poetry install --no-dev
	# Alpine Linux does not ship with ASan nor UBSan, so we can't use them in
	# our test-build. Just use the same build for both. CCache will also speed this up.
	poetry run dagon \
		-o bootstrap-mode=lazy \
		-o jobs=4 \
		-o test-toolchain=tools/gcc-10-static-rel.jsonc \
		--interface simple \
		--fail-cancels \
		test format.check
	poetry run dagon \
		-o main-toolchain=tools/gcc-10-static-rel.jsonc \
		--interface simple \
		--fail-cancels \
		build.main
	mv _build/dds _build/dds-linux-x64

alpine-static-ci:
	docker build \
		--build-arg DDS_USER_UID=$(shell id -u) \
		-t dds-builder \
		-f tools/Dockerfile.alpine \
		tools/
	docker run \
		-t --rm \
		-u $(shell id -u) \
		-v $(PWD):/host -w /host \
		-e CCACHE_DIR=/host/.docker-ccache \
		dds-builder \
		make _alpine-static-ci

vagrant-freebsd-ci:
	vagrant up freebsd11
	vagrant rsync
	vagrant ssh freebsd11 -c '\
		export PATH=$$PATH:$$HOME/.local/bin && \
		cd /vagrant && \
		make full-ci \
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

py-check:
	poetry run mypy tools/dds_ci $(shell find tests/ -name *.py)
	poetry run pylint tools/dds_ci $(shell find tests/ -name *.py)

format:
	poetry run dds-format

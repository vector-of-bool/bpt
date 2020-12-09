.SILENT:

.PHONY: \
	docs docs-server docs-watch docs-sync-server linux-ci macos-ci \
	vagrant-freebsd-ci site alpine-static-ci _alpine-static-ci poetry-setup \
	full-ci dev-build release-build


clean:
	rm -f -vr -- $(shell find -name __pycache__ -type d)
	rm -f -vr -- _build/ _prebuilt/
	rm -f -v -- $(shell find -name "*.stamp" -type f)

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

.poetry.stamp: poetry.lock
	poetry install --no-dev
	touch .poetry.stamp

poetry-setup: .poetry.stamp

full-ci: poetry-setup
	poetry run dds-ci --clean

dev-build: poetry-setup
	poetry run dds-ci --rapid

release-build: poetry-setup
	poetry run dds-ci --no-test

macos-ci: full-ci
	mv _build/dds _build/dds-macos-x64

linux-ci: full-ci
	mv _build/dds _build/dds-linux-x64

_alpine-static-ci:
	poetry install --no-dev
	# Alpine Linux does not ship with ASan nor UBSan, so we can't use them in
	# our test-build. Just use the same build for both. CCache will also speed this up.
	poetry run dds-ci \
		--bootstrap-with=lazy \
		--test-toolchain=tools/gcc-9-static-rel.jsonc \
		--main-toolchain=tools/gcc-9-static-rel.jsonc
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

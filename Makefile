NODE ?= node
NPM ?= npm

.PHONY: build test fmt-check ci clean

build:
	$(NODE) scripts/build.mjs

test:
	$(NODE) --test "tests/node/*.test.js"

fmt-check:
	$(NODE) scripts/check-format.mjs

ci: fmt-check build test

clean:
	rm -rf dist


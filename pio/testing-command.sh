#!/bin/sh
TEST_DIR="$1"
shift 1

if [ -z "$TEST_DIR" ] || [ -z "$1" ]; then
	echo "Usage: $0 <test build dir> <test program> [args...]"
	exit 1
fi

if [ "$CI" = "true" ]; then
	# Delete test-specific intermediate output to avoid CI running out of space
	find "$TEST_DIR" -type f -path '*/test_*/*.o' -print -delete
fi

exec "$@"

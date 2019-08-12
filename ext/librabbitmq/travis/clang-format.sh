#!/bin/bash

set -e

exec clang-format-3.9 -style=file $@

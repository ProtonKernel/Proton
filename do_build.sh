#!/bin/bash

# Wrap to ckbuild.sh
export WP=${WP:-$(realpath $PWD/../)}

bash build/ckbuild.sh "$@"

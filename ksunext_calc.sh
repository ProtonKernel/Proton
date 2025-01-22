#!/bin/bash

REPO_URL="https://github.com/rifsxd/KernelSU-Next"
KERNELSU_DIR="drivers/kernelsu"
MAKEFILE_PATH="$KERNELSU_DIR/kernel/Makefile"
TAG_NAME=$1

if [[ -z "$TAG_NAME" ]]; then
  echo "Please provide a tag name as an argument."
  exit 1
fi

# Save the current working directory
TOP_LEVEL_DIR=$(pwd)

# Check if we're in the kernel top-level folder
if [[ ! -d "drivers" ]]; then
  echo "Warning: Not in the kernel top-level folder. A temporary clone will be made."
  TEMP_DIR=$(mktemp -d)
  git clone $REPO_URL $TEMP_DIR/KernelSU-Next
  cd $TEMP_DIR/KernelSU-Next || exit
else
  if [[ ! -d "$KERNELSU_DIR" ]]; then
    echo "Warning: '$KERNELSU_DIR' does not exist. No modifications will be made."
    exit 1
  fi

  if [[ ! -d "../KernelSU-Next" ]]; then
    echo "Cloning KernelSU-Next..."
    git clone $REPO_URL ../KernelSU-Next
    cd ../KernelSU-Next || exit
  else
    echo "Fetching KernelSU-Next..."
    cd ../KernelSU-Next || exit
    git fetch origin
  fi
  git reset --hard $TAG_NAME || exit
  cd - > /dev/null || exit
fi

# Ensure we're in the correct context (either within kernel directory or temporary clone)
if [[ -d "../KernelSU-Next" ]]; then
  cd ../KernelSU-Next || exit
fi

# Fetch the complete history if the repository is shallow
if [ -f .git/shallow ]; then
  git fetch --unshallow
fi

# Count the number of commits in the repository
KSU_GIT_VERSION=$(git rev-list --count HEAD)

# Calculate the version using the formula
KSU_VERSION=$((10000 + KSU_GIT_VERSION + 200))

# Return to the top-level directory
cd "$TOP_LEVEL_DIR" || exit

# Update the Makefile if we're in the kernel top-level folder and the required directory exists
if [[ -d "$KERNELSU_DIR" ]]; then
  echo "Updating ${MAKEFILE_PATH}..."
  sed -i "/warning \"KSU_GIT_VERSION not defined! It is better to make KernelSU-Next a git submodule!\"/!b;n;c\\ccflags-y \+= -DKSU_VERSION=${KSU_VERSION}" "${MAKEFILE_PATH}"
  echo "-- KernelSU version updated to: $KSU_VERSION"
else
  echo "KSU_GIT_VERSION not defined! Default version will be used."
fi

# Clean up temporary directory if it was used
if [[ -n "$TEMP_DIR" ]]; then
  rm -rf "$TEMP_DIR"
fi

# Set the KSU_VERSION
echo "KSU_VERSION=$KSU_VERSION"

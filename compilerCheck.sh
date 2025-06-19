#!/bin/sh

# Minimum required versions
MIN_GCC_MAJOR=13
MIN_CLANG_MAJOR=10

# Fallback clang versions to try
CLANG_CANDIDATES="clang++-19 clang++-18 clang++-17 clang++-16 clang++-15 clang++-10"

# Find c++ path and real target
CXX_PATH=$(command -v c++)
[ -z "$CXX_PATH" ] && {
  echo "No c++ compiler found in PATH."
  exit 1
}
CXX_REAL=$(readlink -f "$CXX_PATH")

echo "Default 'c++' path: $CXX_PATH"
echo "'c++' resolves to: $CXX_REAL"

# Extract version string
CXX_VERSION=$($CXX_PATH --version | head -n 1)
echo "Version string: $CXX_VERSION"

# Functions to extract versions
get_gcc_major_version() {
  echo "$1" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n 1 | cut -d. -f1
}
get_clang_major_version() {
  echo "$1" | sed -n 's/.*clang.*\s\([0-9]\+\)\..*/\1/p'
}

# Function to try known clang++ versions
try_clang_versions() {
  for candidate in $CLANG_CANDIDATES; do
    if command -v "$candidate" >/dev/null 2>&1; then
      VERSION=$($candidate --version | head -n 1)
      MAJOR=$(get_clang_major_version "$VERSION")
      if [ "$MAJOR" -ge "$MIN_CLANG_MAJOR" ]; then
        echo "Using $candidate (version $MAJOR)"
        export CXX=$candidate
        return 0
      else
        echo "Found $candidate but version $MAJOR is too old"
      fi
    fi
  done
  return 1
}

# Determine compiler type and act accordingly
if echo "$CXX_VERSION" | grep -qi "gcc\|g++" || echo "$CXX_REAL" | grep -qi "gcc\|g++"; then
  COMPILER="GCC"
  MAJOR=$(get_gcc_major_version "$CXX_VERSION")
  echo "Detected compiler: $COMPILER $MAJOR"
  if [ "$MAJOR" -lt "$MIN_GCC_MAJOR" ]; then
    echo "$COMPILER version too old (<$MIN_GCC_MAJOR). Trying newer clang++..."
    if command -v clang++ >/dev/null 2>&1; then
      CLANG_VERSION=$(clang++ --version | head -n 1)
      CLANG_MAJOR=$(get_clang_major_version "$CLANG_VERSION")
      if [ "$CLANG_MAJOR" -ge "$MIN_CLANG_MAJOR" ]; then
        echo "Using clang++ (version $CLANG_MAJOR)"
        export CXX=clang++
      else
        echo "System clang++ version $CLANG_MAJOR is too old. Trying specific versions..."
        try_clang_versions || {
          echo "No suitable clang++ found."
          exit 1
        }
      fi
    else
      try_clang_versions || {
        echo "No clang++ available."
        exit 1
      }
    fi
  else
    export CXX="$CXX_PATH"
  fi

elif echo "$CXX_VERSION" | grep -qi "clang"; then
  COMPILER="Clang"
  MAJOR=$(get_clang_major_version "$CXX_VERSION")
  echo "Detected compiler: $COMPILER $MAJOR"
  if [ "$MAJOR" -lt "$MIN_CLANG_MAJOR" ]; then
    echo "$COMPILER version too old (<$MIN_CLANG_MAJOR). Trying specific versions..."
    try_clang_versions || {
      echo "No suitable clang++ found."
      exit 1
    }
  else
    export CXX="$CXX_PATH"
  fi

else
  echo "Unknown compiler: $CXX_VERSION"
  export CXX="$CXX_PATH"
fi

echo "Final compiler selected: $CXX"

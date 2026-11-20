#!/bin/sh
# Homebrew's vulkan-validationlayers manifest references its library by file name only, which the
# Vulkan loader cannot resolve on macOS. Rewrite it to the absolute path. Idempotent.
# Re-run after `brew upgrade vulkan-validationlayers`.
set -eu

prefix="$(brew --prefix)"
manifest="$(brew --prefix vulkan-validationlayers)/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json"
library="${prefix}/lib/libVkLayer_khronos_validation.dylib"

[ -f "$manifest" ] || { echo "error: manifest not found: $manifest" >&2; exit 1; }
[ -f "$library" ]  || { echo "error: layer library not found: $library" >&2; exit 1; }

sed -i '' "s#\"library_path\": \"[^\"]*libVkLayer_khronos_validation.dylib\"#\"library_path\": \"${library}\"#" "$manifest"
grep -q "\"library_path\": \"${library}\"" "$manifest" || { echo "error: failed to patch $manifest" >&2; exit 1; }
echo "validation layer manifest now points to ${library}"

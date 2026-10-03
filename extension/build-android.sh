#!/usr/bin/env bash
# Android menu for the wowdot GDExtension.
# NDK version must match the default in godot-cpp/tools/android.py.
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ROOT
readonly NDK_VERSION="28.1.13356709"
readonly GDEXTENSION="${ROOT}/shared/wowdot/wowdot.gdextension"
readonly EXTENSION="${ROOT}/extension"
readonly PACKAGE_DIR="${ROOT}/export/wowgd/android"
# Project-local SDK so this NDK does not replace copies used by other projects.
readonly ANDROID_SDK="${ROOT}/.android"

if [[ -z "${JAVA_HOME:-}" && -x /opt/homebrew/opt/openjdk@21/bin/java ]]; then
  JAVA_HOME="/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"
fi

die() {
  printf '[ERROR] %s\n' "$*" >&2
  exit 1
}

jobs() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  else
    sysctl -n hw.ncpu
  fi
}

host_tag() {
  case "$(uname -s)" in
    Darwin) printf 'darwin-x86_64' ;;
    Linux) printf 'linux-x86_64' ;;
    *) die "Android builds are supported on macOS and Linux" ;;
  esac
}

require_java() {
  [[ -n "${JAVA_HOME:-}" && -x "${JAVA_HOME}/bin/java" ]] || die "JDK not found. Install openjdk@21 or set JAVA_HOME"
  export JAVA_HOME
  export PATH="${JAVA_HOME}/bin:${PATH}"
}

sdkmanager_bin() {
  local candidate
  for candidate in \
    "/opt/homebrew/share/android-commandlinetools/cmdline-tools/latest/bin/sdkmanager" \
    "/usr/local/share/android-commandlinetools/cmdline-tools/latest/bin/sdkmanager" \
    "${HOME}/Library/Android/sdk/cmdline-tools/latest/bin/sdkmanager"
  do
    if [[ -x "$candidate" ]]; then
      printf '%s' "$candidate"
      return
    fi
  done
  if command -v sdkmanager >/dev/null 2>&1; then
    command -v sdkmanager
    return
  fi
  die "sdkmanager not found. Install android-commandlinetools"
}

ndk_toolchain() {
  printf '%s' "${ANDROID_SDK}/ndk/${NDK_VERSION}/toolchains/llvm/prebuilt/$(host_tag)"
}

install_sdk() {
  require_java
  mkdir -p "$ANDROID_SDK"
  local tool
  tool="$(sdkmanager_bin)"
  yes | "$tool" --sdk_root="$ANDROID_SDK" --licenses >/dev/null || true
  "$tool" --sdk_root="$ANDROID_SDK" \
    "ndk;${NDK_VERSION}" \
    "platforms;android-35" \
    "build-tools;35.0.0" \
    "platform-tools"
  local toolchain
  toolchain="$(ndk_toolchain)"
  [[ -d "$toolchain" ]] || die "NDK toolchain missing at ${toolchain}"
  printf 'NDK ready: %s\n' "${ANDROID_SDK}/ndk/${NDK_VERSION}"
}

client_data_dir() {
  sed -n 's/^client_data_dir="\(.*\)".*/\1/p' "${ROOT}/clients/wowgd/project.godot"
}

link_mpqs() {
  local from="$1"
  local overwrite="$2"
  local file base
  for file in "$from"/*.mpq "$from"/*.MPQ; do
    [[ -f "$file" ]] || continue
    base="$(basename "$file")"
    if [[ "$overwrite" == no && -e "${PACKAGE_DIR}/Data/${base}" ]]; then
      continue
    fi
    ln -f "$file" "${PACKAGE_DIR}/Data/${base}" 2>/dev/null \
      || cp -c "$file" "${PACKAGE_DIR}/Data/${base}"
  done
}

# The loader opens MPQs from the Data directory and one level of locale folders.
# Hardlinks keep the package from copying several gigabytes on the same disk.
package_resources() {
  local src sub
  src="$(client_data_dir)"
  [[ -d "$src" ]] || die "WoW Data folder not found: ${src}"
  mkdir -p "${PACKAGE_DIR}/Data" "${PACKAGE_DIR}/translations"
  link_mpqs "$src" yes
  for sub in "$src"/*/; do
    [[ -d "$sub" ]] || continue
    link_mpqs "$sub" no
  done
  local packed
  packed="$(find "${PACKAGE_DIR}/Data" -iname '*.mpq' | wc -l | tr -d ' ')"
  [[ "$packed" != 0 ]] || die "no MPQ archives in ${src}"
  cp -f "${ROOT}/translations/wowgd/"*.csv "${PACKAGE_DIR}/translations/"
  cat >"${PACKAGE_DIR}/INSTALL.txt" <<'EOF'
Install the APK, then copy Data and translations into the app files directory:

  /sdcard/Android/data/com.wowgd.client/files/Data
  /sdcard/Android/data/com.wowgd.client/files/translations

Open the app once so Android creates that directory. From this folder, run:

  adb push Data /sdcard/Android/data/com.wowgd.client/files/Data
  adb push translations /sdcard/Android/data/com.wowgd.client/files/translations
EOF
  printf 'Packaged %s MPQ archives and translations in %s\n' "$packed" "$PACKAGE_DIR"
}

godot_bin() {
  local godot="/Applications/Godot.app/Contents/MacOS/Godot"
  [[ -x "$godot" ]] || die "Godot not found at ${godot}"
  printf '%s' "$godot"
}

# 4.7.2.stable.official.<hash> -> 4.7.2.stable, matching Godot's template folder.
godot_template_version() {
  local version
  version="$("$(godot_bin)" --version)"
  printf '%s' "${version%%.official*}"
}

install_export_templates() {
  local version dest tag name tmp url
  version="$(godot_template_version)"
  dest="${HOME}/Library/Application Support/Godot/export_templates/${version}"
  if [[ -f "${dest}/android_debug.apk" && -f "${dest}/android_release.apk" ]]; then
    printf 'Export templates ready: %s\n' "$dest"
    return
  fi
  tag="${version/.stable/-stable}"
  name="Godot_v${tag}_export_templates.tpz"
  tmp="$(mktemp -d)"
  for url in \
    "https://github.com/godotengine/godot/releases/download/${tag}/${name}" \
    "https://gh-proxy.com/https://github.com/godotengine/godot/releases/download/${tag}/${name}"
  do
    printf 'Downloading %s\n' "$url"
    if curl -fL --retry 2 --connect-timeout 20 --speed-time 30 --speed-limit 10240 \
      -o "${tmp}/${name}" "$url"; then
      mkdir -p "$dest"
      unzip -o -j "${tmp}/${name}" \
        "templates/android_debug.apk" \
        "templates/android_release.apk" \
        "templates/version.txt" \
        -d "$dest"
      rm -rf "$tmp"
      [[ -f "${dest}/android_debug.apk" ]] || die "android_debug.apk missing after extract"
      printf 'Installed export templates in %s\n' "$dest"
      return
    fi
  done
  rm -rf "$tmp"
  die "Could not download Godot export templates for ${version}"
}

export_apk() {
  local godot
  godot="$(godot_bin)"
  install_export_templates
  [[ -f "${ROOT}/shared/wowdot/bin/libwowdot.android.template_debug.arm64.so" ]] || die "debug library missing. Choose 2 first"
  mkdir -p "$PACKAGE_DIR"
  "$godot" --headless --path "${ROOT}/clients/wowgd" --export-debug "Android" "${PACKAGE_DIR}/WoWGD.apk"
  [[ -f "${PACKAGE_DIR}/WoWGD.apk" ]] || die "APK export failed"
  printf 'APK: %s\n' "${PACKAGE_DIR}/WoWGD.apk"
}

build_android() {
  local target="$1"
  require_java
  local toolchain
  toolchain="$(ndk_toolchain)"
  [[ -d "$toolchain" ]] || die "NDK ${NDK_VERSION} is not installed. Choose 1 first"
  command -v scons >/dev/null 2>&1 || die "scons not found"
  # ANDROID_HOME selects <sdk>/ndk/<version> inside godot-cpp. Keep it on this project copy.
  ANDROID_HOME="$ANDROID_SDK" scons -C "$EXTENSION" -j"$(jobs)" platform=android "target=${target}" arch=arm64 "ANDROID_HOME=${ANDROID_SDK}"
  printf 'Built %s\n' "${ROOT}/shared/wowdot/bin/libwowdot.android.${target}.arm64.so"
}

register_libraries() {
  [[ -f "$GDEXTENSION" ]] || die "missing ${GDEXTENSION}"
  local debug='android.debug.arm64 = "res://addons/wowdot/bin/libwowdot.android.template_debug.arm64.so"'
  local release='android.release.arm64 = "res://addons/wowdot/bin/libwowdot.android.template_release.arm64.so"'
  if grep -q '^android.debug.arm64 ' "$GDEXTENSION" && grep -q '^android.release.arm64 ' "$GDEXTENSION"; then
    printf 'Android libraries already listed in wowdot.gdextension\n'
    return
  fi
  local tmp
  tmp="$(mktemp)"
  awk -v debug="$debug" -v release="$release" '
    { print }
    $0 == "[libraries]" && !inserted {
      print debug
      print release
      inserted = 1
    }
  ' "$GDEXTENSION" >"$tmp"
  mv "$tmp" "$GDEXTENSION"
  printf 'Wrote Android library paths into wowdot.gdextension\n'
}

menu() {
  cat <<'EOF'
WoWdot Android
  1  Install the NDK and SDK components
  2  Build arm64 debug
  3  Build arm64 release
  4  Register wowdot.gdextension
  5  Run 1-4, download export templates if needed, export the APK, and package Data and translations
EOF
}

run_choice() {
  case "$1" in
    1) install_sdk ;;
    2) build_android template_debug ;;
    3) build_android template_release ;;
    4) register_libraries ;;
    5)
      install_sdk
      build_android template_debug
      build_android template_release
      register_libraries
      export_apk
      package_resources
      ;;
    *) die "choose 1, 2, 3, 4, or 5" ;;
  esac
}

main() {
  local choice="${1:-}"
  if [[ -z "$choice" ]]; then
    menu
    read -r -p "Choose: " choice
  fi
  run_choice "$choice"
}

main "$@"

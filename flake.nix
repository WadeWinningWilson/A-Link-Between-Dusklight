{
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };
  outputs = { self, nixpkgs }:
    let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
      
      # Dependencies that are not packaged in nixpkgs:
      aurora-src = pkgs.fetchFromGitHub {
        owner = "encounter";
        repo = "aurora";
        rev = "63606a43265a3bc18dafd500ab4d7a2108f109e6";
        hash = "sha256-xBvnAwGwNzav67Ac6oUz7RqDUwqgL2bsME3OOMn8Tqw=";
      };
      dawn-src = pkgs.fetchzip {
        url = "https://github.com/encounter/dawn-build/releases/download/v20260423.175430/dawn-linux-x86_64.tar.gz";
        hash = "sha256-HXfKTLHtMPwupnFnaflCARtXVPuS/0PoCePXidjE5xs=";
        stripRoot = false;
      };
      nod-src = pkgs.fetchzip {
        url = "https://github.com/encounter/nod/releases/download/v2.0.0-alpha.8/libnod-linux-x86_64.tar.gz";
        hash = "sha256-mUqvLsbsqaZ+HAjMmHYPYO+MgtanGRTw7Gzn5uXR5rE=";
        stripRoot = false;
      };
      # The version of imgui on nixpkgs does not map cleanly.
      imgui-src = pkgs.fetchFromGitHub {
        owner = "ocornut";
        repo = "imgui";
        rev = "v1.91.9b-docking";
        hash = "sha256-mQOJ6jCN+7VopgZ61yzaCnt4R1QLrW7+47xxMhFRHLQ=";
      };
      sqlite-src = pkgs.fetchzip {
        url = "https://sqlite.org/2026/sqlite-amalgamation-3510300.zip";
        hash = "sha256-pNMR8zxaaqfAzQ0AQBOXMct4usdjey1Q0Gnitg06UhM=";
      };
      rmlui-src = pkgs.fetchzip {
        url = "https://github.com/mikke89/RmlUi/archive/f9b8c9e2935d5df2c7dff2c190d3968e99b0c3dc.tar.gz";
        hash = "sha256-g4O/JZUrrcseOz8o2QJRt+2CeuiLnVeuDJc906xvuIg=";
      };
      # Dusk Actual
      dusk = pkgs.stdenv.mkDerivation {
        name = "dusk";
        src = ./.;
        postUnpack = ''
          mkdir -p $sourceRoot/extern/aurora
          cp -r ${aurora-src}/. $sourceRoot/extern/aurora/
          chmod -R u+w $sourceRoot/extern/aurora
          sed -i '/add_subdirectory(tests)/d' $sourceRoot/extern/aurora/CMakeLists.txt
          # Allow Setting VERSION
          sed -i 's/find_package(Git)/# find_package(Git)/' $sourceRoot/CMakeLists.txt
          sed -i 's/set(DUSK_WC_DESCRIBE "UNKNOWN-VERSION")/# set(DUSK_WC_DESCRIBE "UNKNOWN-VERSION")/' $sourceRoot/CMakeLists.txt
        '';
        # Remove last line to re-enable tests
        cmakeFlags = [
          "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
          "-DFETCHCONTENT_SOURCE_DIR_CXXOPTS=${pkgs.cxxopts.src}"
          "-DFETCHCONTENT_SOURCE_DIR_JSON=${pkgs.nlohmann_json.src}"
          "-DFETCHCONTENT_SOURCE_DIR_DAWN_PREBUILT=${dawn-src}"
          "-DFETCHCONTENT_SOURCE_DIR_XXHASH=${pkgs.xxHash.src}"
          "-DFETCHCONTENT_SOURCE_DIR_FMT=${pkgs.fmt.src}"
          "-DFETCHCONTENT_SOURCE_DIR_TRACY=${pkgs.tracy.src}"
          "-DAURORA_SDL3_PROVIDER=system"
          "-DFETCHCONTENT_SOURCE_DIR_NOD_PREBUILT=${nod-src}"
          "-DAURORA_NOD_PROVIDER=package"
          "-DFETCHCONTENT_SOURCE_DIR_FREETYPE=${pkgs.freetype.src}"
          "-DFETCHCONTENT_SOURCE_DIR_ZSTD=${pkgs.zstd.src}"
          "-DFETCHCONTENT_SOURCE_DIR_SQLITE3=${sqlite-src}"
          "-DFETCHCONTENT_SOURCE_DIR_IMGUI=${imgui-src}"
          "-DFETCHCONTENT_SOURCE_DIR_RMLUI=${rmlui-src}"
          "-DCMAKE_CROSSCOMPILING=ON" # Tests are not working as I didn't want to work through getting google's test suite working as well. This is the only guard I could find to disable it.
          "-DDUSK_WC_DESCRIBE=NixOS-${builtins.substring 0 7 (self.rev or "dirty")}"
          "-DDUSK_VERSION_STRING=NixOS-${builtins.substring 0 7 (self.rev or "dirty")}"
        ];
        installPhase = ''
          mkdir -p $out/bin
          cp dusk $out/bin/dusk
          cp -r ./res $out/bin/res
          
          # Install .desktop file
          mkdir -p $out/share/applications
          cp $src/platforms/freedesktop/dusk.desktop $out/share/applications/dusk.desktop


          for size in 16 32 48 64 128 256 512 1024; do
            install -Dm644 $src/platforms/freedesktop/''${size}x''${size}/apps/dusk.png \
            $out/share/icons/hicolor/''${size}x''${size}/apps/dusk.png
          done
        '';
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.pkg-config
          pkgs.wayland
        ];
        buildInputs = [
          pkgs.libGL
          pkgs.libX11
          pkgs.libXcursor
          pkgs.libxi
          pkgs.libxcb
          pkgs.libxrandr
          pkgs.libxscrnsaver
          pkgs.libxtst
          pkgs.libjpeg8
          pkgs.libxkbcommon
          pkgs.libglvnd
          pkgs.cxxopts
          pkgs.abseil-cpp
          pkgs.sdl3
          pkgs.fmt
          pkgs.tracy
          pkgs.freetype
          pkgs.zstd
          pkgs.libdecor
        ];
      };
    in {
      packages.x86_64-linux.default = dusk;
    };
}

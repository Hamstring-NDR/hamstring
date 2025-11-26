{
  description = "HAMSTRING - High-performance DGA detection pipeline";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        
        # Common build inputs for all C++ modules
        commonBuildInputs = with pkgs; [
          cmake
          pkg-config
          boost
          spdlog
          fmt
          nlohmann_json
          yaml-cpp
          openssl
          curl
          zlib
          cyrus_sasl
          rdkafka
        ];

        # Build a C++ module
        buildModule = { name, src ? ./cpp, extraInputs ? [] }:
          pkgs.stdenv.mkDerivation {
            pname = "hamstring-${name}";
            version = "0.1.0";
            
            inherit src;
            
            nativeBuildInputs = commonBuildInputs ++ extraInputs;
            
            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DBUILD_TESTING=OFF"
            ];
            
            # Don't use default cmake buildPhase, do it manually
            dontUseCmakeBuildDir = true;
            
            configurePhase = ''
              runHook preConfigure
              cmake -B build -S . \
                -DCMAKE_BUILD_TYPE=Release \
                -DBUILD_TESTING=OFF
              runHook postConfigure
            '';
            
            buildPhase = ''
              runHook preBuild
              cmake --build build --target ${name} -j$NIX_BUILD_CORES
              runHook postBuild
            '';
            
            installPhase = ''
              mkdir -p $out/bin
              cp build/src/${name}/${name} $out/bin/
            '';
            
            meta = with pkgs.lib; {
              description = "HAMSTRING ${name} module";
              homepage = "https://github.com/yourusername/hamstring";
              license = licenses.eupl12;
              platforms = platforms.unix;
            };
          };

        # Build OCI image (Docker-compatible) for a module
        buildOciImage = { name, package, config-file ? null }:
          pkgs.dockerTools.buildImage {
            name = "hamstring/${name}";
            tag = "latest";
            
            copyToRoot = pkgs.buildEnv {
              name = "image-root";
              paths = [ package pkgs.coreutils pkgs.bash ];
              pathsToLink = [ "/bin" ];
            };
            
            config = {
              Cmd = [ "${package}/bin/${name}" "/config.yaml" ];
              Env = [
                "PATH=/bin"
              ];
              WorkingDir = "/";
              ExposedPorts = {};
              Labels = {
                "org.opencontainers.image.title" = "HAMSTRING ${name}";
                "org.opencontainers.image.description" = "DGA detection pipeline - ${name} module";
                "org.opencontainers.image.source" = "https://github.com/yourusername/hamstring";
              };
            };
          };

        # Python environment for Zeek and Detector
        pythonEnv = pkgs.python3.withPackages (ps: with ps; [
          # From requirements.zeek.txt
          pyyaml
          click
          colorlog
          # Note: ONNX runtime would go here when available
        ]);

      in
      {
        packages = {
          # C++ module packages
          logserver = buildModule { name = "logserver"; };
          logcollector = buildModule { name = "logcollector"; };
          prefilter = buildModule { name = "prefilter"; };
          inspector = buildModule { name = "inspector"; };
          
          # Zeek network capture (Python-based)
          zeek = pkgs.writeScriptBin "hamstring-zeek" ''
            #!${pkgs.bash}/bin/bash
            export PYTHONPATH=${./src}:$PYTHONPATH
            exec ${pythonEnv}/bin/python -m src.zeek.zeek_handler "$@"
          '';
          
          # OCI images (Docker-compatible)
          oci-logserver = buildOciImage {
            name = "logserver";
            package = self.packages.${system}.logserver;
          };
          
          oci-logcollector = buildOciImage {
            name = "logcollector";
            package = self.packages.${system}.logcollector;
          };
          
          oci-prefilter = buildOciImage {
            name = "prefilter";
            package = self.packages.${system}.prefilter;
          };
          
          oci-inspector = buildOciImage {
            name = "inspector";
            package = self.packages.${system}.inspector;
          };
          
          # Zeek sensor image
          oci-zeek = pkgs.dockerTools.buildImage {
            name = "hamstring/zeek";
            tag = "latest";
            
            copyToRoot = pkgs.buildEnv {
              name = "zeek-root";
              paths = with pkgs; [ 
                zeek
                pythonEnv
                coreutils
                bash
                # Copy source files (Python handlers and Zeek configs)
                (pkgs.runCommand "hamstring-src" {} ''
                  mkdir -p $out/opt/src
                  
                  # Copy Python modules
                  cp -r ${./src/zeek} $out/opt/src/zeek
                  cp -r ${./src/base} $out/opt/src/base
                '')
              ];
            };
            
            config = {
              Cmd = [ "${pythonEnv}/bin/python" "-m" "src.zeek.zeek_handler" "-c" "/config.yaml" ];
              Env = [
                "PYTHONPATH=/opt"
                "PATH=${pkgs.zeek}/bin:${pythonEnv}/bin:/bin"
              ];
              WorkingDir = "/opt";
            };
          };
          
          # Default package builds all C++ modules (not OCI images)
          default = pkgs.buildEnv {
            name = "hamstring-pipeline";
            paths = [
              self.packages.${system}.logserver
              self.packages.${system}.logcollector
              self.packages.${system}.prefilter
              self.packages.${system}.inspector
            ];
          };
        };

        # Apps for easy running
        apps = {
          logserver = {
            type = "app";
            program = "${self.packages.${system}.logserver}/bin/logserver";
          };
          logcollector = {
            type = "app";
            program = "${self.packages.${system}.logcollector}/bin/logcollector";
          };
          prefilter = {
            type = "app";
            program = "${self.packages.${system}.prefilter}/bin/prefilter";
          };
          inspector = {
            type = "app";
            program = "${self.packages.${system}.inspector}/bin/inspector";
          };
          zeek = {
            type = "app";
            program = "${self.packages.${system}.zeek}/bin/hamstring-zeek";
          };
        };

        # Development shell
        devShells.default = pkgs.mkShell {
          buildInputs = commonBuildInputs ++ (with pkgs; [
            # Development tools
            clang-tools
            gdb
            valgrind
            
            # Python for Zeek handler
            pythonEnv
            
            # Zeek for network capture
            zeek
            
            # Kafka for testing
            apacheKafka
            
            # Docker tools for OCI image testing
            skopeo
            podman
          ]);
          
          shellHook = ''
            echo "🚀 HAMSTRING Development Environment"
            echo "======================================"
            echo ""
            echo "Available commands:"
            echo "  cmake -B build -S cpp"
            echo "  cmake --build build -j"
            echo "  python -m src.zeek.zeek_handler -c config.yaml"
            echo ""
            echo "Build OCI images:"
            echo "  nix build .#oci-logserver"
            echo "  nix build .#oci-inspector"
            echo ""
            echo "Load images:"
            echo "  docker load < result"
            echo ""
            echo "C++ Modules:"
            echo "  - LogServer"
            echo "  - LogCollector"
            echo "  - Prefilter  "
            echo "  - Inspector"
            echo ""
          '';
        };

        # NixOS module for deployment
        nixosModules.default = { config, lib, pkgs, ... }:
          with lib;
          let
            cfg = config.services.hamstring;
          in
          {
            options.services.hamstring = {
              enable = mkEnableOption "HAMSTRING DGA detection pipeline";
              
              configFile = mkOption {
                type = types.path;
                description = "Path to config.yaml";
              };
              
              kafkaBrokers = mkOption {
                type = types.listOf types.str;
                default = [ "localhost:19092" ];
                description = "Kafka broker addresses";
              };
              
              modules = {
                logserver.enable = mkEnableOption "LogServer module";
                logcollector.enable = mkEnableOption "LogCollector module";
                prefilter.enable = mkEnableOption "Prefilter module";
                inspector.enable = mkEnableOption "Inspector module";
              };
            };

            config = mkIf cfg.enable {
              systemd.services = {
                hamstring-logserver = mkIf cfg.modules.logserver.enable {
                  description = "HAMSTRING LogServer";
                  after = [ "network.target" "kafka.service" ];
                  wantedBy = [ "multi-user.target" ];
                  
                  serviceConfig = {
                    ExecStart = "${self.packages.${system}.logserver}/bin/logserver ${cfg.configFile}";
                    Restart = "always";
                    User = "hamstring";
                  };
                };
                
                hamstring-logcollector = mkIf cfg.modules.logcollector.enable {
                  description = "HAMSTRING LogCollector";
                  after = [ "network.target" "kafka.service" "hamstring-logserver.service" ];
                  wantedBy = [ "multi-user.target" ];
                  
                  serviceConfig = {
                    ExecStart = "${self.packages.${system}.logcollector}/bin/logcollector ${cfg.configFile}";
                    Restart = "always";
                    User = "hamstring";
                  };
                };
                
                hamstring-prefilter = mkIf cfg.modules.prefilter.enable {
                  description = "HAMSTRING Prefilter";
                  after = [ "network.target" "kafka.service" "hamstring-logcollector.service" ];
                  wantedBy = [ "multi-user.target" ];
                  
                  serviceConfig = {
                    ExecStart = "${self.packages.${system}.prefilter}/bin/prefilter ${cfg.configFile}";
                    Restart = "always";
                    User = "hamstring";
                  };
                };
                
                hamstring-inspector = mkIf cfg.modules.inspector.enable {
                  description = "HAMSTRING Inspector";
                  after = [ "network.target" "kafka.service" "hamstring-prefilter.service" ];
                  wantedBy = [ "multi-user.target" ];
                  
                  serviceConfig = {
                    ExecStart = "${self.packages.${system}.inspector}/bin/inspector ${cfg.configFile}";
                    Restart = "always";
                    User = "hamstring";
                  };
                };
              };
              
              users.users.hamstring = {
                isSystemUser = true;
                group = "hamstring";
                description = "HAMSTRING pipeline user";
              };
              
              users.groups.hamstring = {};
            };
          };
      }
    );
}

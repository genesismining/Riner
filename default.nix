let
  pkgs = import (fetchTarball {
    url = "https://github.com/nixos/nixpkgs/tarball/76adfc180c0e4bad011e33ee73fba2a56e1f212b";
    sha256 = "1rzkmxrardj2319qwm7ivlkypdb50j9ijfvgf17bb02rajdcxdx3";
  }) {};
  optional_lib = fetchTarball {
    url = "https://github.com/akrzemi1/optional/tarball/f6249e7fdcb80131c390a083f1621d96023e72e9";
    sha256 = "0msyv8j4zzf12cccz2355w3ky9dy2myxpx6qn6jz17iz3xqfyy2j";
  };
in
with pkgs;

stdenv.mkDerivation {
  name = "Riner";
  src = ./.;
  makeTarget = "riner";
  doCheck = false;  # 2 tests require network, 1 test requires GPU
  
  nativeBuildInputs = [ cmake ];
  buildInputs = [ 
    boost
    libpqxx
    protobuf 
    opencl-headers_2_2
    ocl-icd
    openssl
    asio_1_12
    easyloggingpp
    microsoft_gsl
    nlohmann_json
    optional_lib
  ];
  checkInputs = [ gtest ];
  
  patchPhase = ''
    rm -rf lib/asio/*
    ln -s ${asio_1_12}/ lib/asio/asio
  
    rm -rf lib/easyloggingpp
    sed -i '/add_subdirectory("lib\/easyloggingpp")/d' CMakeLists.txt

    rm -rf lib/gsl
    sed -i '/add_subdirectory("lib\/gsl")/d' CMakeLists.txt

    rm -rf lib/json
    sed -i '/add_subdirectory("lib\/json")/d' CMakeLists.txt

    rm -rf lib/Optional
    ln -s ${optional_lib} lib/Optional
  '';
  
  installPhase = ''
    mkdir -p $out/bin
    cp ./riner $out/bin/riner
  '';
}

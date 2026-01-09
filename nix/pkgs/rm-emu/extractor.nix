{
  python3Packages,
  fetchFromGitHub,
  makeWrapper,
}:
python3Packages.buildPythonApplication {
  name = "extractor";
  pyproject = false;

  src = fetchFromGitHub {
    owner = "ddvk";
    repo = "stuff";
    rev = "69158ac";
    hash = "sha256-CrP+/BP821xXz4avuGac1+qbNGhWBi1aUkTbpYxB17c=";
  };
  propagatedBuildInputs = with python3Packages; [ protobuf ];

  nativeBuildInputs = [ makeWrapper ];

  installPhase = ''
    mkdir -p $out/bin
    install -Dm755 extractor/extractor.py $out/bin/extractor
    install -Dm755 extractor/update_metadata_pb2.py $out/bin/

    wrapProgram $out/bin/extractor --set PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION python
  '';
}

with import <nixpkgs> {
  crossSystem = (import <nixpkgs/lib>).systems.examples.remarkable2;
};

mkShell {
  buildInputs = []; # your dependencies here
}

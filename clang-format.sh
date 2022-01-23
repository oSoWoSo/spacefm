#!/bin/bash

# src
find ./src -maxdepth 1 -type f -o -iname *.hxx -o -iname *.cxx -o -iname *.ixx | xargs clang-format -i
find ./src/ptk -maxdepth 1 -type f -o -iname *.hxx -o -iname *.cxx -o -iname *.ixx | xargs clang-format -i
find ./src/vfs -maxdepth 1 -type f -o -iname *.hxx -o -iname *.cxx -o -iname *.ixx | xargs clang-format -i
find ./src/mime-type -maxdepth 1 -type f -o -iname *.hxx -o -iname *.cxx -o -iname *.ixx | xargs clang-format -i

# vendor
# find ./src/vendor/alphanum -maxdepth 1 -type f -o -iname *.hxx -o -iname *.cxx -o -iname *.ixx | xargs clang-format -i
# find ./src/vendor/ztd -maxdepth 1 -type f -o -iname *.hxx -o -iname *.cxx -o -iname *.ixx | xargs clang-format -i

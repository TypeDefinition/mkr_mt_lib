#!/bin/bash

rm -r docs
mkdir docs
doxygen doxyfile
sensible-browser ./docs/html/index.html

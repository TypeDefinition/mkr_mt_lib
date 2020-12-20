#!/bin/bash

rm -r docs
doxygen doxyfile
sensible-browser ./docs/html/index.html

#!/bin/bash

bazel clean --expunge
bazel build //:mkr_mt_lib
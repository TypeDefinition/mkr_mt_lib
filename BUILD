cc_library(
    name = "mkr_mt_lib",
    hdrs = glob(["mt/**/*.h"]),
    srcs = glob(["mt/**/*.cpp"]),
    copts = ["-std=c++20"],
    linkopts = ["-lpthread"],
    deps = ["@mkr_common_lib//:mkr_common_lib"],
    visibility = ["//visibility:public"],
)

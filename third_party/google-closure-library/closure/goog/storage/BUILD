load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "collectablestorage",
    srcs = ["collectablestorage.js"],
    deps = [
        ":errorcode",
        ":expiringstorage",
        ":richstorage",
        "//closure/goog/array",
        "//closure/goog/iter",
        "//closure/goog/storage/mechanism:iterablemechanism",
    ],
)

closure_js_library(
    name = "collectablestoragetester",
    testonly = False,
    srcs = ["collectablestoragetester.js"],
    deps = [
        ":collectablestorage",
        "//closure/goog/storage/mechanism:iterablemechanism",
        "//closure/goog/testing:asserts",
        "//closure/goog/testing:mockclock",
    ],
)

closure_js_library(
    name = "encryptedstorage",
    srcs = ["encryptedstorage.js"],
    deps = [
        ":collectablestorage",
        ":errorcode",
        ":richstorage",
        "//closure/goog/crypt",
        "//closure/goog/crypt:arc4",
        "//closure/goog/crypt:base64",
        "//closure/goog/crypt:sha1",
        "//closure/goog/json",
        "//closure/goog/storage/mechanism:iterablemechanism",
    ],
)

closure_js_library(
    name = "errorcode",
    srcs = ["errorcode.js"],
)

closure_js_library(
    name = "expiringstorage",
    srcs = ["expiringstorage.js"],
    deps = [
        ":richstorage",
        "//closure/goog/storage/mechanism",
    ],
)

closure_js_library(
    name = "richstorage",
    srcs = ["richstorage.js"],
    deps = [
        ":errorcode",
        ":storage",
        "//closure/goog/storage/mechanism",
    ],
)

closure_js_library(
    name = "storage",
    srcs = ["storage.js"],
    deps = [
        ":errorcode",
        "//closure/goog/json",
        "//closure/goog/storage/mechanism",
    ],
)

closure_js_library(
    name = "storagetester",
    testonly = False,
    srcs = ["storagetester.js"],
    deps = [
        ":storage",
        "//closure/goog/structs:map",
        "//closure/goog/testing:asserts",
    ],
)

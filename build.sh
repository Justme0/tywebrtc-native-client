
find qt_proj -not -path "qt_proj/third_party/*" | egrep ".+\.(cc|cpp|h)$" | xargs clang-format -i || true

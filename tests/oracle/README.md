# Oracle files

One `eNN.expected.csv` per project, written by that project's author, with at least
the columns `rule` and `model` (the full log shape is also accepted). `tests/oracle_check`
compares the project's replay log against it row by row; the project registers the
test with `edgellm_add_oracle_test` in its own `CMakeLists.txt`, and CI runs it with the
stub. The stub's model column is deterministic from the grammar alone (see
`common/c/src/edgellm_stub.c`); a real-engine log is compared on the bench, not here.

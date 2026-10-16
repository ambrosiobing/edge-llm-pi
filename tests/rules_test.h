/* rules_test.h: how a project adds cases to the shared rule harness.
 *
 * In projects/eNN-slug/src/rules_eNN_test.c:
 *
 *     #include "rules_test.h"
 *     static void cases(void)
 *     {
 *         io_event ev = { 0.0, "vl53l4cd", 1, { 250.0 } };
 *         rules_test_expect("e06", "250 mm once is APPROACHING", &ev, 1);
 *         io_event hist[2] = { { 0.0, "vl53l4cd", 1, { 250.0 } },
 *                              { 1.0, "vl53l4cd", 1, { 240.0 } } };
 *         rules_test_expect_hist("e06", "below 300 mm twice is TOO_CLOSE",
 *                                hist, 2, &ev, 2);
 *     }
 *     RULES_TEST_REGISTER(cases);
 *
 * and list the file as RULES_TEST in edgellm_add_project. The expected value is the
 * index into the rule's name table, which is also the grammar's literal order.
 */
#ifndef EDGELLM_RULES_TEST_H
#define EDGELLM_RULES_TEST_H

#include "io.h"
#include "rules.h"

void rules_test_expect(const char *project, const char *label, const io_event *ev,
                       int expected);
void rules_test_expect_hist(const char *project, const char *label, const io_event *hist,
                            int n_hist, const io_event *ev, int expected);

#define RULES_TEST_REGISTER(fn) \
    __attribute__((constructor)) static void rules_test_register_##fn(void) { fn(); }

#endif /* EDGELLM_RULES_TEST_H */

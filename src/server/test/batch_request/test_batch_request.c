#include <check.h>
#include <stdio.h>
#include <stdlib.h>

START_TEST(test_one)
  {
  }
END TEST





START_TEST(test_two)
  {
  }
END_TEST




Suite *batch_request_suite(void)
  {
  Suite *s = suite_create("batch_request suite methods");

  TCase tc_core = tcase_create("test_one");
  tcase_add_test(tc_core, test_one);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("test_two");
  tcase_add_test(tc_core, test_one);
  suite_add_tcase(s, tc_core);

  return(s);
  }




void rundebug()
  {
  }




int main(void)
  {
  int number_failed = 0;
  SRunner *sr = NULL;
  rundebug();
  sr = srunner_create(batch_request_suite());
  srunner_set_log(sr, "batch_request_suite.log");
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return(number_failed);
  }

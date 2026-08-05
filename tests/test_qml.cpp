#include <QtQuickTest/quicktest.h>

int main(int argc, char **argv) {
  QTEST_SET_MAIN_SOURCE_PATH
  return quick_test_main(argc, argv, "amt_qml", AMT_QML_TEST_SOURCE_DIR);
}

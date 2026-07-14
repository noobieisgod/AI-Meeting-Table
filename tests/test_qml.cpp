#include <QtQuickTest/quicktest.h>

#include <QFile>
#include <QTemporaryDir>

#include <cstdlib>

int main(int argc, char **argv) {
  QTEST_SET_MAIN_SOURCE_PATH

  QTemporaryDir testDirectory;
  if (!testDirectory.isValid()) {
    return EXIT_FAILURE;
  }

  const QString testPath = testDirectory.filePath("tst_transcript_scroll.qml");
  const QString helperPath = testDirectory.filePath("TranscriptScroll.js");
  if (!QFile::copy(QStringLiteral(AMT_QML_TEST_SOURCE), testPath) ||
      !QFile::copy(QStringLiteral(AMT_TRANSCRIPT_SCROLL_SOURCE), helperPath)) {
    return EXIT_FAILURE;
  }

  const QByteArray sourceDirectory = QFile::encodeName(testDirectory.path());
  return quick_test_main(argc, argv, "amt_qml", sourceDirectory.constData());
}

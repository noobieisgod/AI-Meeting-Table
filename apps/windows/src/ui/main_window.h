#pragma once

#include <QMainWindow>
#include <QHash>

#include "app/application_context.h"

class QListWidget;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QLineEdit;
class QLayout;
class QVBoxLayout;
class QSplitter;
class QScrollArea;
class QTextBrowser;
class QTextEdit;
class QListWidgetItem;
class QWidget;

namespace amt {

class MeetingTableWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ApplicationContext *context, QWidget *parent = nullptr);

private slots:
    void refreshUi();
    void handleCreateTable();
    void handleRunSession();
    void handleSelectionChanged();
    void handleSessionStateChanged(const amt::SessionState &state);
    void handleContinuationRequested(const QString &tableId, const QString &reason, int limitKind);
    void handleLogVisibilityChanged(bool checked);
    void handleSearchChanged(const QString &text);
    void handleOpenSettings();
    void handleSeatClicked(const QString &seatId, int seatIndex);
    void handlePauseResume();
    void handleSendMessage();
    void handleAddAttachment();
    void handleArtifactActivated(QListWidgetItem *item);
    void handleTableContextMenu(const QPoint &pos);
    void handleArtifactContextMenu(const QPoint &pos);
    void handleTranscriptAction(const QUrl &url);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    SessionState *currentState();
    void rebuildTableList();
    void renderRightPane(const SessionState &state);
    bool applySeatEdit(SessionState &state, int seatIndex, const SeatConfig &seat, QString *error = nullptr);
    void updateRunPauseButtons();
    void renameTable(SessionState &state);
    void deleteTable(SessionState &state);
    void appendUserMessage(SessionState &state, const QString &message);
    void updateBranding();
    void restoreSplitterState();
    void saveSplitterState() const;
    bool shouldPersistSessionUpdate(const SessionState &state, bool *shouldRebuildTableList = nullptr);
    void appendTranscriptEntries(const SessionState &state, int startIndex);
    void renderAttachmentChips(const SessionState &state);
    void removeAttachmentById(SessionState &state, const QString &attachmentId);
    void resetTranscriptRendering();

    ApplicationContext *m_context;
    QString m_searchText;
    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;
    QSplitter *m_transcriptSplitter;
    QListWidget *m_tableList;
    QLineEdit *m_searchEdit;
    MeetingTableWidget *m_meetingTableWidget;
    QLabel *m_phaseLabel;
    QLabel *m_roundLabel;
    QLabel *m_activeLabel;
    QLabel *m_budgetLabel;
    QLabel *m_sessionTimeLabel;
    QPushButton *m_runButton;
    QPushButton *m_pauseButton;
    QPushButton *m_settingsButton;
    QPushButton *m_logToggleButton;
    QScrollArea *m_transcriptScrollArea;
    QWidget *m_transcriptContainer;
    QVBoxLayout *m_transcriptLayout;
    QWidget *m_attachmentChipContainer;
    QLayout *m_attachmentChipLayout;
    QListWidget *m_artifactList;
    QPlainTextEdit *m_logView;
    QTextEdit *m_messageComposer;
    QPushButton *m_sendButton;
    QPushButton *m_addAttachmentButton;
    QWidget *m_logPanel;
    QHash<QString, QByteArray> m_lastSessionFingerprints;
    QHash<QString, int> m_lastElapsedPersists;
    QString m_renderedTranscriptTableId;
    QStringList m_renderedTranscriptEntryIds;
    QStringList m_renderedTranscriptEntrySignatures;
};

} // namespace amt

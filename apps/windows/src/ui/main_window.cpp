#include "ui/main_window.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLayout>
#include <QLayoutItem>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSizePolicy>
#include <QSplitter>
#include <QTimer>
#include <QTextEdit>
#include <QTextCursor>
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QTextOption>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#include "ui/dialogs/create_table_dialog.h"
#include "ui/dialogs/seat_editor_dialog.h"
#include "ui/dialogs/settings_dialog.h"
#include "ui/widgets/meeting_table_widget.h"

#include <algorithm>

namespace amt {

namespace {

class FlowLayout final : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent = nullptr, int margin = 0, int hSpacing = 6, int vSpacing = 6)
        : QLayout(parent),
          m_hSpacing(hSpacing),
          m_vSpacing(vSpacing)
    {
        setContentsMargins(margin, margin, margin, margin);
    }

    ~FlowLayout() override
    {
        QLayoutItem *item = nullptr;
        while ((item = takeAt(0)) != nullptr) {
            delete item;
        }
    }

    void addItem(QLayoutItem *item) override
    {
        m_items.append(item);
    }

    int count() const override
    {
        return m_items.size();
    }

    QLayoutItem *itemAt(int index) const override
    {
        return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
    }

    QLayoutItem *takeAt(int index) override
    {
        return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
    }

    Qt::Orientations expandingDirections() const override
    {
        return {};
    }

    bool hasHeightForWidth() const override
    {
        return true;
    }

    int heightForWidth(int width) const override
    {
        return doLayout(QRect(0, 0, width, 0), true);
    }

    QSize minimumSize() const override
    {
        QSize size;
        for (const auto *item : m_items) {
            size = size.expandedTo(item->minimumSize());
        }
        const auto margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }

    QSize sizeHint() const override
    {
        return minimumSize();
    }

    void setGeometry(const QRect &rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const
    {
        const auto margins = contentsMargins();
        QRect effectiveRect = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;

        for (auto *item : m_items) {
            const QSize hint = item->sizeHint();
            const int nextX = x + hint.width() + m_hSpacing;
            if (lineHeight > 0 && nextX - m_hSpacing > effectiveRect.right() + 1) {
                x = effectiveRect.x();
                y += lineHeight + m_vSpacing;
                lineHeight = 0;
            }

            if (!testOnly) {
                item->setGeometry(QRect(QPoint(x, y), hint));
            }
            x += hint.width() + m_hSpacing;
            lineHeight = qMax(lineHeight, hint.height());
        }

        return (y + lineHeight - rect.y()) + margins.bottom();
    }

    QList<QLayoutItem *> m_items;
    int m_hSpacing;
    int m_vSpacing;
};

class AttachmentChipButton final : public QToolButton
{
public:
    explicit AttachmentChipButton(QWidget *parent = nullptr)
        : QToolButton(parent)
    {
        setAutoRaise(false);
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        setCursor(Qt::PointingHandCursor);
        setStyleSheet(
            "QToolButton {"
            " padding: 4px 10px;"
            " border: 1px solid rgba(127,127,127,0.35);"
            " border-radius: 14px;"
            " background: rgba(127,127,127,0.10);"
            " }"
            "QToolButton::menu-indicator { image: none; width: 0; }");
    }
};

class TranscriptEntryView final : public QTextBrowser
{
public:
    explicit TranscriptEntryView(QWidget *parent = nullptr)
        : QTextBrowser(parent)
    {
        setReadOnly(true);
        setOpenLinks(false);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        document()->setDocumentMargin(0);
        connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
                this, [this](const QSizeF &) { refreshHeight(); });
    }

    void setTranscriptHtml(const QString &html)
    {
        setHtml(html);
        refreshHeight();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QTextBrowser::resizeEvent(event);
        refreshHeight();
    }

private:
    void refreshHeight()
    {
        document()->setTextWidth(viewport()->width());
        const int height = qCeil(document()->size().height()) + contentsMargins().top() + contentsMargins().bottom() + 6;
        setFixedHeight(qMax(24, height));
    }
};

class TranscriptPhaseBadge final : public QFrame
{
public:
    explicit TranscriptPhaseBadge(const QString &text, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        setStyleSheet(
            "QFrame {"
            " padding: 4px 10px;"
            " border: 1px solid rgba(127,127,127,0.35);"
            " border-radius: 14px;"
            " background: rgba(127,127,127,0.10);"
            " }"
            "QLabel { background: transparent; border: none; font-weight: 700; }");
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *label = new QLabel(text, this);
        layout->addWidget(label);
    }
};

QString formatElapsed(int totalSeconds)
{
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString transcriptEntrySignature(const SessionState &state, const TranscriptEntry &entry)
{
    return QString("%1|%2|%3|%4|%5|%6|%7")
        .arg(entry.entryId,
             entry.timestamp.toUTC().toString(Qt::ISODate),
             QString::number(static_cast<int>(entry.phase)),
             QString::number(entry.round),
             state.queuedInputIds.contains(entry.entryId) ? "queued" : "live",
             entry.speakerSeatId,
             entry.content);
}

QString transcriptActorLabel(const SessionState &state, const TranscriptEntry &entry)
{
    QString actor = entry.speakerName;
    if (entry.isUser) {
        actor = "You";
    } else if (entry.isDecision) {
        actor += " (Decision Maker)";
    } else {
        for (const auto &seat : state.seats) {
            if (seat.seatId == entry.speakerSeatId && seat.role != Role::None) {
                actor += QString(" (%1)").arg(displaySeatRole(seat.role));
                break;
            }
        }
    }
    return actor;
}

QString transcriptEntryHtml(const SessionState &state, const TranscriptEntry &entry)
{
    const QString timestamp = entry.timestamp.isValid()
        ? entry.timestamp.toLocalTime().toString("HH:mm:ss")
        : "--:--:--";
    const QString actor = transcriptActorLabel(state, entry).toHtmlEscaped();
    const bool queued = entry.isUser && state.queuedInputIds.contains(entry.entryId);
    QString metaSuffix;
    if (queued) {
        metaSuffix += " <em>(Next Round)</em>";
    }
    if (entry.isUser) {
        metaSuffix += QString(" <a href=\"edit:%1\">Edit</a> <a href=\"delete:%1\">Delete</a>")
            .arg(entry.entryId.toHtmlEscaped());
    }

    QTextDocument markdownDocument;
    markdownDocument.setDocumentMargin(0);
    markdownDocument.setMarkdown(entry.content, QTextDocument::MarkdownDialectGitHub);
    QString bodyHtml = markdownDocument.toHtml();
    const int bodyStart = bodyHtml.indexOf("<body");
    if (bodyStart >= 0) {
        const int contentStart = bodyHtml.indexOf('>', bodyStart);
        const int bodyEnd = bodyHtml.indexOf("</body>", contentStart);
        if (contentStart >= 0 && bodyEnd > contentStart) {
            bodyHtml = bodyHtml.mid(contentStart + 1, bodyEnd - contentStart - 1);
        }
    }
    bodyHtml = bodyHtml.trimmed();
    if (bodyHtml.isEmpty()) {
        bodyHtml = QString("<p>%1</p>").arg(entry.content.toHtmlEscaped().replace('\n', "<br/>"));
    }

    return QString(
               "<table width=\"100%%\" cellspacing=\"0\" cellpadding=\"0\" "
               "style=\"border-collapse: collapse; margin: 0;\">"
               "<tr><td style=\"padding: 0;\">"
               "<div style=\"display: block; margin: 0 0 8px 0; font-weight: 700;\">[%1] %2%3</div>"
               "<div style=\"display: block; margin: 0;\">%4</div>"
               "</td></tr>"
               "</table>")
        .arg(timestamp.toHtmlEscaped(),
             actor,
             metaSuffix,
             bodyHtml);
}

QString logEventTypeLabel(LogEventType type)
{
    switch (type) {
    case LogEventType::SessionStarted: return "Session";
    case LogEventType::UserMessageAdded: return "User";
    case LogEventType::PhaseStarted: return "Phase";
    case LogEventType::TurnStarted: return "Turn";
    case LogEventType::AISpoke: return "AI";
    case LogEventType::AISkipped: return "Skip";
    case LogEventType::ProviderCallFailed: return "Provider";
    case LogEventType::RetryScheduled: return "Retry";
    case LogEventType::PhaseEnded: return "Phase";
    case LogEventType::FinalDecisionMade: return "Decision";
    case LogEventType::SessionStopped: return "Stop";
    case LogEventType::LimitReached: return "Limit";
    }
    return "Log";
}

bool sessionCanStart(const SessionState &state, QString *error = nullptr)
{
    int participantCount = 0;
    for (const auto &seat : state.seats) {
        if (!seat.occupied || !seat.enabled) {
            continue;
        }
        if (!hasConcreteModelSelection(seat)) {
            if (error) {
                *error = QString("%1 needs a concrete model selection before the session can run.")
                    .arg(displaySeatName(seat));
            }
            return false;
        }
        if (seat.role != Role::FinalDecisionMaker) {
            participantCount += 1;
        }
    }

    const QString roleError = validateSeatRoleAssignments(state.seats);
    if (!roleError.isEmpty()) {
        if (error) {
            *error = roleError;
        }
        return false;
    }

    if (participantCount == 0) {
        if (error) {
            *error = "At least one occupied non-final participant is required to run a meeting.";
        }
        return false;
    }

    bool hasUserMessage = false;
    for (const auto &entry : state.transcript) {
        if (entry.isUser && !entry.content.trimmed().isEmpty()) {
            hasUserMessage = true;
            break;
        }
    }
    if (!hasUserMessage) {
        if (error) {
            *error = "Send a user message in the transcript pane before starting the session.";
        }
        return false;
    }

    return true;
}

QWidget *makePanelContainer(const QString &title, QWidget *content, QWidget *parent = nullptr)
{
    auto *panel = new QFrame(parent);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *titleLabel = new QLabel(title, panel);
    QFont font = titleLabel->font();
    font.setBold(true);
    titleLabel->setFont(font);
    layout->addWidget(titleLabel);
    layout->addWidget(content, 1);
    return panel;
}

bool useLightBranding(const QPalette &palette)
{
    return palette.window().color().lightnessF() < 0.45;
}

QByteArray sessionFingerprint(const amt::SessionState &state)
{
    QJsonObject object{
        {"title", state.title},
        {"pinned", state.pinned},
        {"phase", toString(state.phase)},
        {"round", state.round},
        {"activeSeatId", state.activeSeatId},
        {"paused", state.paused},
        {"pauseRequested", state.pauseRequested},
        {"continuationPending", state.continuationPending},
        {"continuationLimitKind", state.continuationLimitKind},
        {"continuationReason", state.continuationReason},
        {"waitingForNextTurn", state.waitingForNextTurn},
        {"arbitrationSatisfied", state.arbitrationSatisfied},
        {"usedTokens", state.usedTokens},
        {"usedCost", state.usedCost},
        {"phaseUsedTokens", state.phaseUsedTokens},
        {"phaseUsedCost", state.phaseUsedCost},
        {"pendingResearchResponses", state.pendingResearchResponses},
        {"currentArtifactVersionId", state.currentArtifactVersionId},
        {"queuedInputCount", state.queuedInputIds.size()},
        {"attachmentCount", state.attachments.size()},
        {"artifactCount", state.artifacts.size()},
        {"transcriptCount", state.transcript.size()},
        {"logCount", state.log.size()}
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QImage tintedBrandImage(QImage image, const QPalette &palette)
{
    image = image.convertToFormat(QImage::Format_ARGB32);
    if (useLightBranding(palette)) {
        const QColor tint("#f5f1e6");
        for (int y = 0; y < image.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const QColor source = QColor::fromRgba(line[x]);
                if (source.alpha() == 0) {
                    continue;
                }
                QColor remapped = tint;
                remapped.setAlpha(source.alpha());
                line[x] = remapped.rgba();
            }
        }
    }
    return image;
}

QIcon brandIcon(const QPalette &palette)
{
    QImage image(":/branding/icon_logo.png");
    if (image.isNull()) {
        return {};
    }
    return QIcon(QPixmap::fromImage(tintedBrandImage(std::move(image), palette)));
}

QIcon attachmentChipIcon(const QString &filePath)
{
    static QFileIconProvider provider;
    return provider.icon(QFileInfo(filePath));
}

}

MainWindow::MainWindow(ApplicationContext *context, QWidget *parent)
    : QMainWindow(parent),
      m_context(context),
      m_mainSplitter(new QSplitter(Qt::Horizontal, this)),
      m_rightSplitter(new QSplitter(Qt::Vertical, this)),
      m_transcriptSplitter(new QSplitter(Qt::Vertical, this)),
      m_tableList(new QListWidget(this)),
      m_searchEdit(new QLineEdit(this)),
      m_meetingTableWidget(new MeetingTableWidget(this)),
      m_phaseLabel(new QLabel(this)),
      m_roundLabel(new QLabel(this)),
      m_activeLabel(new QLabel(this)),
      m_budgetLabel(new QLabel(this)),
      m_sessionTimeLabel(new QLabel(this)),
      m_runButton(new QPushButton("Run Session", this)),
      m_pauseButton(new QPushButton("Pause", this)),
      m_settingsButton(new QPushButton("Settings", this)),
      m_transcriptScrollArea(new QScrollArea(this)),
      m_transcriptContainer(new QWidget(this)),
      m_transcriptLayout(new QVBoxLayout()),
      m_attachmentChipContainer(new QWidget(this)),
      m_attachmentChipLayout(new FlowLayout()),
      m_artifactList(new QListWidget(this)),
      m_logView(new QPlainTextEdit(this)),
      m_messageComposer(new QTextEdit(this)),
      m_sendButton(new QPushButton("Send", this)),
      m_addAttachmentButton(new QPushButton("Add Attachment", this))
{
    setWindowTitle("AI Meeting Table");
    const QRect availableGeometry = qApp && qApp->primaryScreen()
        ? qApp->primaryScreen()->availableGeometry()
        : QRect(0, 0, 1560, 940);
    const QSize defaultSize(1560, 940);
    const QSize clampedSize(qMin(defaultSize.width(), qMax(960, availableGeometry.width() - 48)),
                            qMin(defaultSize.height(), qMax(720, availableGeometry.height() - 48)));
    const QPoint centeredTopLeft(availableGeometry.left() + qMax(0, (availableGeometry.width() - clampedSize.width()) / 2),
                                 availableGeometry.top() + qMax(0, (availableGeometry.height() - clampedSize.height()) / 2));
    setGeometry(QRect(centeredTopLeft, clampedSize));

    auto *leftSidebar = new QFrame(this);
    leftSidebar->setObjectName("leftSidebar");
    leftSidebar->setMinimumWidth(250);
    auto *leftLayout = new QVBoxLayout(leftSidebar);
    leftLayout->setContentsMargins(10, 10, 10, 10);
    leftLayout->setSpacing(8);

    auto *leftTitle = new QLabel("Meeting Tables", leftSidebar);
    QFont leftTitleFont = leftTitle->font();
    leftTitleFont.setBold(true);
    leftTitle->setFont(leftTitleFont);
    leftLayout->addWidget(leftTitle);

    m_searchEdit->setPlaceholderText("Search tables");
    leftLayout->addWidget(m_searchEdit);

    m_tableList->setContextMenuPolicy(Qt::CustomContextMenu);
    leftLayout->addWidget(m_tableList, 1);

    auto *createButton = new QPushButton("Create Table", leftSidebar);
    leftLayout->addWidget(createButton);

    auto *centerWorkspace = new QFrame(this);
    centerWorkspace->setObjectName("centerWorkspace");
    centerWorkspace->setMinimumWidth(560);
    auto *centerLayout = new QVBoxLayout(centerWorkspace);
    centerLayout->setContentsMargins(12, 12, 12, 12);
    centerLayout->setSpacing(10);

    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(10);
    auto makeStatusCard = [centerWorkspace, statusRow](const QString &title, QLabel **outValue) {
        auto *card = new QFrame(centerWorkspace);
        card->setObjectName("topStatusCard");
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(12, 8, 12, 8);
        layout->setSpacing(2);

        auto *titleLabel = new QLabel(title, card);
        titleLabel->setObjectName("topStatusTitle");
        titleLabel->setStyleSheet("background: transparent; border: none;");
        titleLabel->setAttribute(Qt::WA_StyledBackground, false);
        layout->addWidget(titleLabel);

        auto *valueLabel = new QLabel(card);
        valueLabel->setObjectName("topStatusValue");
        valueLabel->setStyleSheet("background: transparent; border: none;");
        valueLabel->setAttribute(Qt::WA_StyledBackground, false);
        layout->addWidget(valueLabel);

        statusRow->addWidget(card, 1);
        *outValue = valueLabel;
    };
    makeStatusCard("Phase", &m_phaseLabel);
    makeStatusCard("Round", &m_roundLabel);
    makeStatusCard("Active", &m_activeLabel);
    makeStatusCard("Budget", &m_budgetLabel);
    makeStatusCard("Session Time", &m_sessionTimeLabel);
    centerLayout->addLayout(statusRow);

    centerLayout->addWidget(m_meetingTableWidget, 1);

    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    actionRow->addWidget(m_runButton);
    actionRow->addWidget(m_pauseButton);
    actionRow->addStretch(1);
    actionRow->addWidget(m_settingsButton);
    m_logToggleButton = new QPushButton("Show Log", this);
    m_logToggleButton->setCheckable(true);
    actionRow->addWidget(m_logToggleButton);
    centerLayout->addLayout(actionRow);

    auto *rightSidebar = new QFrame(this);
    rightSidebar->setObjectName("rightSidebar");
    rightSidebar->setMinimumWidth(340);
    auto *rightLayout = new QVBoxLayout(rightSidebar);
    rightLayout->setContentsMargins(10, 10, 10, 10);
    rightLayout->setSpacing(8);

    m_transcriptLayout->setContentsMargins(0, 0, 0, 0);
    m_transcriptLayout->setSpacing(0);
    m_transcriptLayout->addStretch(1);
    m_transcriptContainer->setLayout(m_transcriptLayout);
    m_transcriptScrollArea->setWidgetResizable(true);
    m_transcriptScrollArea->setFrameShape(QFrame::NoFrame);
    m_transcriptScrollArea->setWidget(m_transcriptContainer);
    m_attachmentChipContainer->setLayout(m_attachmentChipLayout);
    m_attachmentChipContainer->setVisible(false);
    m_attachmentChipContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    m_messageComposer->setAcceptRichText(false);
    m_messageComposer->setPlaceholderText("Tell the meeting what you want to achieve.");
    m_messageComposer->setMinimumHeight(45);
    m_messageComposer->installEventFilter(this);
    m_messageComposer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto *transcriptContent = new QWidget(rightSidebar);
    auto *transcriptLayout = new QVBoxLayout(transcriptContent);
    transcriptLayout->setContentsMargins(0, 0, 0, 0);
    transcriptLayout->setSpacing(6);

    auto *transcriptHistoryPanel = new QWidget(transcriptContent);
    auto *historyLayout = new QVBoxLayout(transcriptHistoryPanel);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    historyLayout->setSpacing(4);
    historyLayout->addWidget(new QLabel("Messages", transcriptHistoryPanel));
    historyLayout->addWidget(m_transcriptScrollArea, 1);

    auto *composerPanel = new QWidget(transcriptContent);
    auto *composerLayout = new QVBoxLayout(composerPanel);
    composerLayout->setContentsMargins(0, 0, 0, 0);
    composerLayout->setSpacing(6);
    composerLayout->addWidget(new QLabel("Input", composerPanel));
    composerLayout->addWidget(m_attachmentChipContainer);
    composerLayout->addWidget(m_messageComposer);
    auto *composerActions = new QHBoxLayout();
    composerActions->setSpacing(8);
    composerActions->addWidget(m_addAttachmentButton);
    composerActions->addStretch(1);
    composerActions->addWidget(m_sendButton);
    composerLayout->addLayout(composerActions);

    m_transcriptSplitter->addWidget(transcriptHistoryPanel);
    m_transcriptSplitter->addWidget(composerPanel);
    m_transcriptSplitter->setCollapsible(0, false);
    m_transcriptSplitter->setCollapsible(1, false);
    m_transcriptSplitter->setStretchFactor(0, 4);
    m_transcriptSplitter->setStretchFactor(1, 2);
    transcriptLayout->addWidget(m_transcriptSplitter, 1);

    m_artifactList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_logView->setReadOnly(true);
    m_logView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_logView->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    m_logPanel = makePanelContainer("Event Log", m_logView, rightSidebar);

    m_rightSplitter->addWidget(makePanelContainer("Transcript", transcriptContent, rightSidebar));
    m_rightSplitter->addWidget(makePanelContainer("Artifacts", m_artifactList, rightSidebar));
    m_rightSplitter->addWidget(m_logPanel);
    m_rightSplitter->setCollapsible(0, false);
    m_rightSplitter->setCollapsible(1, false);
    m_rightSplitter->setCollapsible(2, false);
    rightLayout->addWidget(m_rightSplitter, 1);

    m_mainSplitter->addWidget(leftSidebar);
    m_mainSplitter->addWidget(centerWorkspace);
    m_mainSplitter->addWidget(rightSidebar);
    m_mainSplitter->setCollapsible(0, false);
    m_mainSplitter->setCollapsible(1, false);
    m_mainSplitter->setCollapsible(2, false);
    setCentralWidget(m_mainSplitter);

    connect(createButton, &QPushButton::clicked, this, &MainWindow::handleCreateTable);
    connect(m_runButton, &QPushButton::clicked, this, &MainWindow::handleRunSession);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::handlePauseResume);
    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::handleSendMessage);
    connect(m_addAttachmentButton, &QPushButton::clicked, this, &MainWindow::handleAddAttachment);
    connect(m_settingsButton, &QPushButton::clicked, this, &MainWindow::handleOpenSettings);
    connect(m_logToggleButton, &QPushButton::toggled, this, &MainWindow::handleLogVisibilityChanged);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::handleSearchChanged);
    connect(m_tableList, &QListWidget::currentRowChanged, this, &MainWindow::handleSelectionChanged);
    connect(m_tableList, &QListWidget::customContextMenuRequested, this, &MainWindow::handleTableContextMenu);
    connect(m_artifactList, &QListWidget::itemDoubleClicked, this, &MainWindow::handleArtifactActivated);
    connect(m_artifactList, &QListWidget::customContextMenuRequested, this, &MainWindow::handleArtifactContextMenu);
    connect(m_context->sessionRunner(), &SessionRunner::sessionStateChanged, this, &MainWindow::handleSessionStateChanged);
    connect(m_context->sessionRunner(), &SessionRunner::continuationRequested, this, &MainWindow::handleContinuationRequested);
    connect(m_meetingTableWidget, &MeetingTableWidget::seatClicked, this, &MainWindow::handleSeatClicked);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { saveSplitterState(); });

    rebuildTableList();
    restoreSplitterState();
    QString targetTableId = QSettings().value("ui/currentTableId").toString();
    const bool savedTableExists = std::any_of(m_context->tables().cbegin(), m_context->tables().cend(), [&](const auto &table) {
        return table && table->tableId == targetTableId;
    });
    if (!savedTableExists) {
        ApplicationContext::SessionHandle newestWithContent;
        ApplicationContext::SessionHandle newest;
        for (const auto &table : m_context->tables()) {
            if (!table) {
                continue;
            }
            const bool hasContent = !table->transcript.isEmpty() || !table->artifacts.isEmpty() || !table->log.isEmpty();
            if (hasContent && (!newestWithContent || table->updatedAt > newestWithContent->updatedAt)) {
                newestWithContent = table;
            }
            if (!newest || table->updatedAt > newest->updatedAt) {
                newest = table;
            }
        }
        const auto selected = newestWithContent ? newestWithContent : newest;
        targetTableId = selected ? selected->tableId : QString();
    }
    if (!targetTableId.isEmpty()) {
        for (int i = 0; i < m_tableList->count(); ++i) {
            if (m_tableList->item(i)->data(Qt::UserRole).toString() == targetTableId) {
                m_tableList->setCurrentRow(i);
                break;
            }
        }
    }
    if (m_tableList->count() > 0 && !m_tableList->currentItem()) {
        m_tableList->setCurrentRow(0);
    }
    updateBranding();
    refreshUi();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_messageComposer && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return QMainWindow::eventFilter(obj, event);
            } else {
                handleSendMessage();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

SessionState *MainWindow::currentState()
{
    const auto *item = m_tableList->currentItem();
    if (!item) {
        return nullptr;
    }

    const QString tableId = item->data(Qt::UserRole).toString();
    for (const auto &state : m_context->tables()) {
        if (state && state->tableId == tableId) {
            return state.get();
        }
    }
    return nullptr;
}

void MainWindow::refreshUi()
{
    SessionState *state = currentState();
    if (!state) {
        m_phaseLabel->setText("None");
        m_roundLabel->setText("-");
        m_activeLabel->setText("None");
        m_budgetLabel->setText("0 tokens");
        m_sessionTimeLabel->setText("00:00");
        resetTranscriptRendering();
        renderAttachmentChips({});
        m_artifactList->clear();
        m_logView->clear();
        m_meetingTableWidget->setSessionState({});
        updateRunPauseButtons();
        return;
    }

    m_phaseLabel->setText(toString(state->phase));
    m_roundLabel->setText(QString::number(state->round));
    m_activeLabel->setText(state->activeSeatId.isEmpty() ? "None" : state->activeSeatId);
    m_budgetLabel->setText(QString("%1 / %2 tokens")
        .arg(state->usedTokens)
        .arg(state->budgetPolicy.maxTotalTokens));
    m_budgetLabel->setToolTip(QString("Total tokens: %1\nPhase tokens: %2")
                                  .arg(state->usedTokens)
                                  .arg(state->phaseUsedTokens));
    m_sessionTimeLabel->setText(formatElapsed(state->elapsedSeconds));
    if (state->continuationPending && (state->paused || state->phase == Phase::Paused)) {
        m_sessionTimeLabel->setToolTip(QString("Meeting paused for continuation.\n%1").arg(state->continuationReason));
    } else {
        m_sessionTimeLabel->setToolTip(state->waitingForNextTurn
                                           ? "Waiting 5 seconds before the next model starts."
                                           : QString());
    }

    for (const auto &seat : state->seats) {
        if (seat.seatId == state->activeSeatId) {
            m_activeLabel->setText(displaySeatName(seat));
            break;
        }
    }

    updateBranding();
    renderRightPane(*state);
    m_meetingTableWidget->setSessionState(*state);
    m_logPanel->setVisible(state->logVisible); // Issue #5: wire logVisible to panel
    QSignalBlocker blocker(m_logToggleButton);
    m_logToggleButton->setChecked(state->logVisible);
    m_logToggleButton->setText(state->logVisible ? "Hide Log" : "Show Log");
    updateRunPauseButtons();
}

void MainWindow::handleCreateTable()
{
    CreateTableDialog dialog(m_context->modelCatalogManager(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    SessionState state = dialog.tableDefinition();
    m_context->applyEffectiveBudgetPolicy(state);
    m_context->save(state);
    rebuildTableList();
    for (int i = 0; i < m_tableList->count(); ++i) {
        if (m_tableList->item(i)->data(Qt::UserRole).toString() == state.tableId) {
            m_tableList->setCurrentRow(i);
            break;
        }
    }
    refreshUi();
}

void MainWindow::handleRunSession()
{
    auto *state = currentState();
    if (!state) {
        return;
    }

    if (state->continuationPending && (state->paused || state->phase == Phase::Paused)) {
        handleContinuationRequested(state->tableId, state->continuationReason, state->continuationLimitKind);
        return;
    }

    if (state->paused || state->phase == Phase::Paused) {
        m_context->sessionRunner()->resumeSession(*state);
        m_context->save(*state);
        refreshUi();
        return;
    }

    if (isRunningPhase(state->phase)) {
        QMessageBox::information(this, "Run Session", "The session is already running. Use Pause if you want to interrupt it after the current round.");
        return;
    }

    if (state->phase == Phase::Completed || state->phase == Phase::Stopped || state->phase == Phase::Failed) {
        if (QMessageBox::question(this, "Restart Session",
                "This session has already ended. Do you want to restart fresh and clear its history?\n\nChoose No to continue the same meeting context instead.",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) == QMessageBox::Yes) {
            state->transcript.clear();
            state->log.clear();
            state->artifacts.clear();
            state->queuedInputIds.clear();
            state->usedTokens = 0;
            state->usedCost = 0.0;
            state->elapsedSeconds = 0;
            state->phaseElapsedSeconds = 0;
            state->round = 1;
            state->execQcLoopCount = 0;
            for (auto &usage : state->seatUsage) {
                usage.totalTokens = 0;
                usage.totalCost = 0.0;
                usage.phaseTokens = 0;
                usage.phaseCost = 0.0;
            }
            state->currentArtifactVersionId.clear();
        }
    }

    QString error;
    if (!sessionCanStart(*state, &error)) {
        QMessageBox::warning(this, "Run Session", error);
        return;
    }

    m_context->sessionRunner()->startSession(*state);
    m_context->save(*state);
    rebuildTableList();
    refreshUi();
}

void MainWindow::handleSelectionChanged()
{
    if (const auto *item = m_tableList->currentItem()) {
        QSettings().setValue("ui/currentTableId", item->data(Qt::UserRole).toString());
    }
    refreshUi();
}

void MainWindow::handleSessionStateChanged(const amt::SessionState &state)
{
    // Issue #3: Update the shared handle in-place instead of deep-copying
    for (auto &table : m_context->tables()) {
        if (table && table->tableId == state.tableId) {
            *table = state;
            m_context->applyEffectiveBudgetPolicy(*table);
            bool shouldRebuildTableList = false;
            if (shouldPersistSessionUpdate(*table, &shouldRebuildTableList)) {
                m_context->save(*table);
            }
            if (shouldRebuildTableList) {
                rebuildTableList();
            }
            break;
        }
    }
    refreshUi();
}

void MainWindow::handleContinuationRequested(const QString &tableId, const QString &reason, int limitKindValue)
{
    SessionState *state = nullptr;
    for (auto &table : m_context->tables()) {
        if (table && table->tableId == tableId) {
            state = table.get();
            break;
        }
    }
    if (!state) {
        return;
    }

    const auto limitKind = static_cast<BudgetLimitKind>(limitKindValue);
    const auto choice = QMessageBox::question(
        this,
        "Continue Meeting?",
        QString("%1\n\nDo you want this meeting to continue anyway?").arg(reason),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (choice != QMessageBox::Yes) {
        m_context->sessionRunner()->stopSession(*state, reason);
        m_context->save(*state);
        rebuildTableList();
        refreshUi();
        return;
    }

    m_context->sessionRunner()->grantContinuation(*state, limitKind);
    m_context->sessionRunner()->resumeSession(*state);
    m_context->save(*state);
    rebuildTableList();
    refreshUi();
}

void MainWindow::handleLogVisibilityChanged(bool checked)
{
    if (auto *state = currentState()) {
        state->logVisible = checked;
        m_context->save(*state);
        refreshUi();
    }
}

void MainWindow::handleSearchChanged(const QString &text)
{
    m_searchText = text.trimmed();
    rebuildTableList();
}

void MainWindow::handleOpenSettings()
{
    const QString currentId = currentState() ? currentState()->tableId : QString();
    SettingsDialog dialog(m_context->credentialStore(), m_context->modelCatalogManager(), &m_context->appSettings(), &m_context->tables(), currentId, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    for (const auto &table : m_context->tables()) {
        if (!table) {
            continue;
        }
        m_context->applyEffectiveBudgetPolicy(*table);
        m_context->save(*table);
    }
    m_context->saveAppSettings();
    m_context->applyTheme();
    rebuildTableList();
    refreshUi();
}

void MainWindow::handleSeatClicked(const QString &seatId, int seatIndex)
{
    Q_UNUSED(seatId);
    auto *state = currentState();
    if (!state) {
        return;
    }

    QVector<SeatConfig> seats = hasPendingSeatChanges(*state) ? state->pendingSeats : state->seats;
    while (seats.size() < 8) {
        SeatConfig seat;
        seat.seatId = QString("seat-%1").arg(seats.size() + 1);
        seats.append(seat);
    }

    SeatEditorDialog dialog(seats.value(seatIndex), seatIndex, m_context->modelCatalogManager(), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString error;
    if (!applySeatEdit(*state, seatIndex, dialog.editedSeat(), &error)) {
        QMessageBox::warning(this, "Edit Seat", error);
        return;
    }

    m_context->save(*state);
    rebuildTableList();
    refreshUi();
}

void MainWindow::handlePauseResume()
{
    auto *state = currentState();
    if (!state) {
        return;
    }

    if (state->continuationPending && (state->paused || state->phase == Phase::Paused)) {
        QMessageBox::information(this, "Meeting Paused", "This meeting is waiting for a continue/stop decision. Use Continue Meeting to resume it.");
        return;
    }

    if (state->paused || state->phase == Phase::Paused) {
        m_context->sessionRunner()->resumeSession(*state);
    } else if (isRunningPhase(state->phase) || state->waitingForNextTurn) {
        m_context->sessionRunner()->requestPause(*state);
    } else {
        QMessageBox::information(this, "Pause Session", "There is no active session to pause right now.");
        return;
    }
    m_context->save(*state);
    refreshUi();
}

void MainWindow::handleSendMessage()
{
    auto *state = currentState();
    if (!state) {
        return;
    }

    const QString message = m_messageComposer->toPlainText().trimmed();
    if (message.isEmpty()) {
        return;
    }

    if (isRunningPhase(state->phase) && !state->paused) {
        QMessageBox::information(this, "Send Message", "Pause the meeting first if you want to add a new instruction before the next round.");
        return;
    }

    appendUserMessage(*state, message);
    m_messageComposer->clear();
    if (state->phase == Phase::Completed || state->phase == Phase::Stopped || state->phase == Phase::Failed) {
        QString error;
        if (sessionCanStart(*state, &error)) {
            m_context->sessionRunner()->startSession(*state);
        } else if (!error.isEmpty()) {
            QMessageBox::warning(this, "Continue Meeting", error);
        }
    }
    m_context->save(*state);
    rebuildTableList();
    refreshUi();
}

void MainWindow::handleAddAttachment()
{
    auto *state = currentState();
    if (!state) {
        return;
    }

    const QStringList files = QFileDialog::getOpenFileNames(this, "Add Attachments");
    if (files.isEmpty()) {
        return;
    }

    bool changed = false;
    for (const auto &filePath : files) {
        QString error;
        AttachmentRecord attachment = m_context->uploadManager()->createAttachment(filePath, &error);
        if (attachment.attachmentId.isEmpty()) {
            QMessageBox::warning(this, "Add Attachment", error.isEmpty() ? "The attachment could not be read." : error);
            continue;
        }
        bool exists = false;
        for (auto &existing : state->attachments) {
            if (existing.fileHash == attachment.fileHash) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        state->attachments.append(attachment);
        if (isRunningPhase(state->phase) || state->waitingForNextTurn) {
            state->queuedInputIds.append(attachment.attachmentId);
        }
        changed = true;
    }

    if (!changed) {
        QMessageBox::information(this, "Add Attachment", "Those files are already attached to this meeting.");
        return;
    }

    m_context->save(*state);
    refreshUi();
}

void MainWindow::handleArtifactActivated(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    QString content;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        content = QString::fromUtf8(file.readAll());
    } else {
        content = QString("Artifact content unavailable.\n\nPath: %1").arg(path);
    }

    QDialog dialog(this);
    dialog.setWindowTitle(item->text());
    dialog.resize(760, 620);
    auto *layout = new QVBoxLayout(&dialog);
    auto *viewer = new QTextBrowser(&dialog);
    viewer->setOpenExternalLinks(false);
    QTextDocument markdownDocument;
    markdownDocument.setMarkdown(content, QTextDocument::MarkdownDialectGitHub);
    viewer->setHtml(markdownDocument.toHtml());
    viewer->setToolTip(path);
    layout->addWidget(viewer, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::handleTableContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_tableList->itemAt(pos);
    if (!item) {
        return;
    }

    const QString tableId = item->data(Qt::UserRole).toString();
    SessionState *target = nullptr;
    for (const auto &state : m_context->tables()) {
        if (state && state->tableId == tableId) {
            target = state.get();
            break;
        }
    }
    if (!target) {
        return;
    }

    QMenu menu(this);
    QAction *duplicateAction = menu.addAction("Duplicate");
    QAction *renameAction = menu.addAction("Rename");
    QAction *pinAction = menu.addAction(target->pinned ? "Unpin" : "Pin");
    QAction *deleteAction = menu.addAction("Delete");
    QAction *chosen = menu.exec(m_tableList->viewport()->mapToGlobal(pos));
    if (!chosen) {
        return;
    }

    if (chosen == duplicateAction) {
        SessionState newState = *target;
        newState.tableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        newState.title = target->title + " (Copy)";
        newState.updatedAt = QDateTime::currentDateTimeUtc();
        newState.phase = Phase::Idle;
        newState.round = 0;
        newState.execQcLoopCount = 0;
        newState.elapsedSeconds = 0;
        newState.usedTokens = 0;
        newState.usedCost = 0.0;
        newState.transcript.clear();
        newState.log.clear();
        newState.artifacts.clear();
        newState.attachments.clear();
        newState.queuedInputIds.clear();
        newState.currentArtifactVersionId.clear();
        for (auto &usage : newState.seatUsage) {
            usage.totalTokens = 0;
            usage.totalCost = 0.0;
        }
        newState.activeSeatId.clear();
        newState.paused = false;
        newState.pauseRequested = false;
        newState.pausedResumePhase = Phase::Idle;
        newState.continuationPending = false;
        newState.continuationLimitKind = 0;
        newState.continuationReason.clear();
        newState.waitingForNextTurn = false;
        newState.arbitrationSatisfied = false;
        m_context->save(newState);
    } else if (chosen == renameAction) {
        renameTable(*target);
    } else if (chosen == pinAction) {
        target->pinned = !target->pinned;
        m_context->save(*target);
    } else if (chosen == deleteAction) {
        deleteTable(*target);
        return;
    }

    rebuildTableList();
    refreshUi();
}

void MainWindow::handleArtifactContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_artifactList->itemAt(pos);
    if (!item) {
        return;
    }

    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }

    QMenu menu(this);
    QAction *openAction = menu.addAction("Open");
    QAction *revealAction = menu.addAction("Reveal");
    QAction *chosen = menu.exec(m_artifactList->viewport()->mapToGlobal(pos));
    if (chosen == openAction) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else if (chosen == revealAction) {
        const QFileInfo fileInfo(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
    }
}

void MainWindow::handleTranscriptAction(const QUrl &url)
{
    SessionState *state = currentState();
    if (!state) {
        return;
    }

    const QString action = url.scheme();
    const QString entryId = url.toString().mid(action.length() + 1);

    if (action == "delete") {
        auto it = std::remove_if(state->transcript.begin(), state->transcript.end(), [&](const TranscriptEntry &e) { return e.entryId == entryId; });
        if (it != state->transcript.end()) {
            state->transcript.erase(it, state->transcript.end());
            state->queuedInputIds.removeAll(entryId);
            m_context->save(*state);
            refreshUi();
        }
    } else if (action == "edit") {
        for (auto &entry : state->transcript) {
            if (entry.entryId == entryId) {
                bool ok;
                QString newText = QInputDialog::getMultiLineText(this, "Edit Message", "Message content:", entry.content, &ok);
                if (ok && !newText.isEmpty()) {
                    entry.content = newText;
                    m_context->save(*state);
                    refreshUi();
                }
                break;
            }
        }
    }
}

void MainWindow::rebuildTableList()
{
    const QString selectedId = currentState() ? currentState()->tableId : QString();
    QVector<const SessionState *> filtered;
    filtered.reserve(m_context->tables().size());
    for (const auto &state : m_context->tables()) {
        if (!state) {
            continue;
        }
        if (!m_searchText.isEmpty() && !state->title.contains(m_searchText, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.append(state.get());
    }

    std::sort(filtered.begin(), filtered.end(), [](const SessionState *lhs, const SessionState *rhs) {
        if (lhs->pinned != rhs->pinned) {
            return lhs->pinned && !rhs->pinned;
        }
        if (lhs->updatedAt != rhs->updatedAt) {
            return lhs->updatedAt > rhs->updatedAt;
        }
        return lhs->title.toLower() < rhs->title.toLower();
    });

    m_tableList->blockSignals(true);
    m_tableList->clear();
    int selectedRow = -1;
    for (int i = 0; i < filtered.size(); ++i) {
        const auto *state = filtered.at(i);
        auto *item = new QListWidgetItem(state->title, m_tableList);
        item->setData(Qt::UserRole, state->tableId);
        item->setToolTip(QString("%1%2")
                             .arg(state->pinned ? "[Pinned] " : "")
                             .arg(state->updatedAt.toLocalTime().toString("yyyy-MM-dd hh:mm")));
        const QString pinPrefix = state->pinned ? "[Pinned] " : "";
        item->setText(QString("%1%2 [%3]").arg(pinPrefix).arg(state->title).arg(toString(state->phase)));
        if (state->tableId == selectedId) {
            selectedRow = i;
        }
    }
    if (selectedRow >= 0) {
        m_tableList->setCurrentRow(selectedRow);
    } else if (m_tableList->count() > 0) {
        m_tableList->setCurrentRow(0);
    }
    m_tableList->blockSignals(false);
}

void MainWindow::renderRightPane(const SessionState &state)
{
    auto *transcriptBar = m_transcriptScrollArea->verticalScrollBar();
    const bool transcriptWasAtBottom = transcriptBar->value() >= transcriptBar->maximum() - 4;
    const int previousTranscriptScroll = transcriptBar->value();

    QStringList entryIds;
    QStringList entrySignatures;
    entryIds.reserve(state.transcript.size());
    entrySignatures.reserve(state.transcript.size());
    for (const auto &entry : state.transcript) {
        entryIds.append(entry.entryId);
        entrySignatures.append(transcriptEntrySignature(state, entry));
    }

    bool needsFullRebuild = m_renderedTranscriptTableId != state.tableId
        || m_renderedTranscriptEntryIds.size() > entryIds.size();
    int appendStartIndex = needsFullRebuild ? 0 : m_renderedTranscriptEntryIds.size();
    if (!needsFullRebuild) {
        for (int i = 0; i < m_renderedTranscriptEntryIds.size(); ++i) {
            if (m_renderedTranscriptEntryIds.at(i) != entryIds.at(i)
                || m_renderedTranscriptEntrySignatures.at(i) != entrySignatures.at(i)) {
                needsFullRebuild = true;
                appendStartIndex = 0;
                break;
            }
        }
    }

    if (needsFullRebuild) {
        resetTranscriptRendering();
        m_renderedTranscriptTableId = state.tableId;
        appendTranscriptEntries(state, 0);
    } else if (appendStartIndex < state.transcript.size()) {
        appendTranscriptEntries(state, appendStartIndex);
    }

    m_renderedTranscriptTableId = state.tableId;
    m_renderedTranscriptEntryIds = entryIds;
    m_renderedTranscriptEntrySignatures = entrySignatures;

    QTimer::singleShot(0, this, [this, transcriptWasAtBottom, previousTranscriptScroll]() {
        if (!m_transcriptScrollArea) {
            return;
        }
        auto *bar = m_transcriptScrollArea->verticalScrollBar();
        if (transcriptWasAtBottom) {
            bar->setValue(bar->maximum());
        } else {
            bar->setValue(qMin(previousTranscriptScroll, bar->maximum()));
        }
    });

    renderAttachmentChips(state);

    m_artifactList->clear();
    for (const auto &artifact : state.artifacts) {
        const QString line = QString("%1  |  %2  |  Round %3")
            .arg(artifact.summary,
                 artifact.createdAt.toLocalTime().toString("yyyy-MM-dd hh:mm"),
                 QString::number(artifact.createdByRound));
        auto *item = new QListWidgetItem(line, m_artifactList);
        item->setData(Qt::UserRole, artifact.filePath);
        item->setToolTip(artifact.filePath);
    }

    const auto *logBar = m_logView->verticalScrollBar();
    const bool logWasAtBottom = logBar->value() >= logBar->maximum() - 4;

    QStringList logLines;
    for (const auto &entry : state.log) {
        const QString timestamp = entry.timestamp.isValid()
            ? entry.timestamp.toLocalTime().toString("HH:mm:ss")
            : "--:--:--";
        const QString actor = entry.actorName.trimmed().isEmpty()
            ? QString()
            : QString(" | %1").arg(entry.actorName.trimmed());
        logLines.append(QString("[%1] [%2]%3 %4")
                            .arg(timestamp,
                                 logEventTypeLabel(entry.type),
                                 actor,
                                 entry.summary));
    }
    m_logView->setPlainText(logLines.join("\n"));

    if (logWasAtBottom) {
        m_logView->moveCursor(QTextCursor::End);
    }
}

bool MainWindow::applySeatEdit(SessionState &state, int seatIndex, const SeatConfig &seat, QString *error)
{
    QVector<SeatConfig> targetSeats = hasPendingSeatChanges(state) ? state.pendingSeats : state.seats;
    while (targetSeats.size() < 8) {
        SeatConfig emptySeat;
        emptySeat.seatId = QString("seat-%1").arg(targetSeats.size() + 1);
        targetSeats.append(emptySeat);
    }

    SeatConfig updatedSeat = seat;
    updatedSeat.seatId = QString("seat-%1").arg(seatIndex + 1);
    normalizeSeatModel(updatedSeat);
    targetSeats[seatIndex] = updatedSeat;

    const QString validationError = validateSeatRoleAssignments(targetSeats);
    if (!validationError.isEmpty()) {
        if (error) {
            *error = validationError;
        }
        return false;
    }

    if (isRunningPhase(state.phase)) {
        state.pendingSeats = targetSeats;
    } else {
        state.seats = targetSeats;
        state.pendingSeats.clear();
        state.finalDecisionMakerSeatId = findFinalDecisionMakerSeatId(state.seats);
    }
    return true;
}

void MainWindow::updateRunPauseButtons()
{
    const SessionState *state = currentState();
    if (!state) {
        m_runButton->setEnabled(false);
        m_pauseButton->setEnabled(false);
        return;
    }

    const bool paused = state->paused || state->phase == Phase::Paused;
    const bool pausedForContinuation = paused && state->continuationPending;
    m_runButton->setEnabled(true);
    m_runButton->setText(pausedForContinuation ? "Continue Meeting"
                                               : (paused ? "Resume Session" : "Run Session"));
    m_pauseButton->setEnabled(!paused && (isRunningPhase(state->phase) || state->waitingForNextTurn));
    m_pauseButton->setText(paused ? "Paused" : "Pause");
}

void MainWindow::renameTable(SessionState &state)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Rename Meeting Table", "Table name", QLineEdit::Normal, state.title, &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    state.title = name;
    m_context->save(state);
}

void MainWindow::deleteTable(SessionState &state)
{
    const QString title = state.title;
    if (QMessageBox::question(this,
                              "Delete Meeting Table",
                              QString("Delete \"%1\" and all of its transcript, log, and artifact history?").arg(title))
        != QMessageBox::Yes) {
        return;
    }

    const QString deletedId = state.tableId;
    m_context->sessionRunner()->discardSession(deletedId);
    if (!m_context->removeTable(deletedId)) {
        QMessageBox::warning(this, "Delete Meeting Table", "The meeting table could not be deleted.");
        return;
    }

    rebuildTableList();
    if (m_tableList->count() > 0 && !m_tableList->currentItem()) {
        m_tableList->setCurrentRow(0);
    }
    refreshUi();
}

void MainWindow::appendUserMessage(SessionState &state, const QString &message)
{
    TranscriptEntry entry;
    entry.entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.tableId = state.tableId;
    entry.phase = state.phase;
    entry.round = state.round;
    entry.speakerSeatId = "user";
    entry.speakerName = "You";
    entry.isUser = true;
    entry.isDecision = false;
    entry.content = message;
    entry.timestamp = QDateTime::currentDateTimeUtc();
    state.transcript.append(entry);
    if (isRunningPhase(state.phase) || state.waitingForNextTurn) {
        state.queuedInputIds.append(entry.entryId);
    }

    LogEvent log;
    log.logId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    log.tableId = state.tableId;
    log.type = LogEventType::UserMessageAdded;
    log.actorName = "You";
    log.phase = state.phase;
    log.round = state.round;
    log.timestamp = QDateTime::currentDateTimeUtc();
    log.summary = "User added new instructions.";
    state.log.append(log);
}

void MainWindow::updateBranding()
{
    const QPalette pal = palette();
    const QIcon icon = brandIcon(pal);
    if (!icon.isNull()) {
        setWindowIcon(icon);
        qApp->setWindowIcon(icon);
    }
}

void MainWindow::restoreSplitterState()
{
    QSettings settings;
    const QByteArray mainState = settings.value("ui/mainSplitter").toByteArray();
    if (!mainState.isEmpty()) {
        m_mainSplitter->restoreState(mainState);
    } else {
        m_mainSplitter->setSizes({280, 760, 520});
    }

    const QByteArray rightState = settings.value("ui/rightWorkspaceSplitter").toByteArray();
    if (!rightState.isEmpty()) {
        m_rightSplitter->restoreState(rightState);
    } else {
        m_rightSplitter->setSizes({430, 180, 180});
    }

    const QByteArray transcriptState = settings.value("ui/transcriptWorkspaceSplitter").toByteArray();
    if (!transcriptState.isEmpty() && m_transcriptSplitter->restoreState(transcriptState)) {
        return;
    }
    m_transcriptSplitter->setSizes({360, 180});
}

void MainWindow::saveSplitterState() const
{
    QSettings settings;
    settings.setValue("ui/mainSplitter", m_mainSplitter->saveState());
    settings.setValue("ui/rightWorkspaceSplitter", m_rightSplitter->saveState());
    settings.setValue("ui/transcriptWorkspaceSplitter", m_transcriptSplitter->saveState());
}

void MainWindow::appendTranscriptEntries(const SessionState &state, int startIndex)
{
    if (startIndex < 0 || startIndex >= state.transcript.size()) {
        return;
    }

    Phase previousPhase = Phase::Idle;
    if (startIndex > 0) {
        previousPhase = state.transcript.at(startIndex - 1).phase;
    }

    for (int i = startIndex; i < state.transcript.size(); ++i) {
        const auto &entry = state.transcript.at(i);
        if (entry.phase != Phase::Idle && entry.phase != previousPhase) {
            auto *phaseRow = new QWidget(m_transcriptContainer);
            auto *phaseLayout = new QVBoxLayout(phaseRow);
            phaseLayout->setContentsMargins(0, 10, 0, 10);
            phaseLayout->setSpacing(0);
            phaseLayout->addWidget(new TranscriptPhaseBadge(QString("%1 Phase").arg(toString(entry.phase)), phaseRow), 0, Qt::AlignLeft);
            m_transcriptLayout->insertWidget(m_transcriptLayout->count() - 1, phaseRow);
        }
        auto *entryView = new TranscriptEntryView(m_transcriptContainer);
        entryView->setTranscriptHtml(transcriptEntryHtml(state, entry));
        connect(entryView, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
            if (url.scheme() == "edit" || url.scheme() == "delete") {
                handleTranscriptAction(url);
            } else {
                QDesktopServices::openUrl(url);
            }
        });
        auto *entryRow = new QWidget(m_transcriptContainer);
        auto *entryLayout = new QVBoxLayout(entryRow);
        entryLayout->setContentsMargins(0, 8, 0, 8);
        entryLayout->setSpacing(0);
        entryLayout->addWidget(entryView);
        m_transcriptLayout->insertWidget(m_transcriptLayout->count() - 1, entryRow);
        previousPhase = entry.phase;
    }
}

void MainWindow::renderAttachmentChips(const SessionState &state)
{
    while (m_attachmentChipLayout->count() > 0) {
        QLayoutItem *item = m_attachmentChipLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    for (const auto &attachment : state.attachments) {
        const bool queued = state.queuedInputIds.contains(attachment.attachmentId);
        auto *chip = new AttachmentChipButton(m_attachmentChipContainer);
        chip->setIcon(attachmentChipIcon(attachment.filePath));
        chip->setText(queued ? QString("%1 • Next Round").arg(attachment.displayName) : attachment.displayName);
        chip->setToolTip(queued ? QString("%1\nQueued for the next round").arg(attachment.filePath) : attachment.filePath);
        chip->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString attachmentId = attachment.attachmentId;
        connect(chip, &QWidget::customContextMenuRequested, this, [this, attachmentId, chip](const QPoint &pos) {
            auto *state = currentState();
            if (!state) {
                return;
            }
            QMenu menu(this);
            QAction *removeAction = menu.addAction("Remove");
            QAction *chosen = menu.exec(chip->mapToGlobal(pos));
            if (chosen == removeAction) {
                removeAttachmentById(*state, attachmentId);
            }
        });
        m_attachmentChipLayout->addWidget(chip);
    }

    m_attachmentChipContainer->setVisible(!state.attachments.isEmpty());
}

void MainWindow::removeAttachmentById(SessionState &state, const QString &attachmentId)
{
    const auto newEnd = std::remove_if(state.attachments.begin(), state.attachments.end(),
                                       [&attachmentId](const AttachmentRecord &attachment) {
                                           return attachment.attachmentId == attachmentId;
                                       });
    if (newEnd == state.attachments.end()) {
        return;
    }
    state.attachments.erase(newEnd, state.attachments.end());
    state.queuedInputIds.removeAll(attachmentId);
    m_context->save(state);
    refreshUi();
}

void MainWindow::resetTranscriptRendering()
{
    while (m_transcriptLayout->count() > 1) {
        QLayoutItem *item = m_transcriptLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_renderedTranscriptTableId.clear();
    m_renderedTranscriptEntryIds.clear();
    m_renderedTranscriptEntrySignatures.clear();
}
bool MainWindow::shouldPersistSessionUpdate(const SessionState &state, bool *shouldRebuildTableList)
{
    const QByteArray fingerprint = sessionFingerprint(state);
    const QByteArray previousFingerprint = m_lastSessionFingerprints.value(state.tableId);
    const bool materialChange = previousFingerprint != fingerprint;
    const int previousElapsed = m_lastElapsedPersists.value(state.tableId, -30);
    const bool elapsedCheckpoint = state.elapsedSeconds - previousElapsed >= 30;

    m_lastSessionFingerprints.insert(state.tableId, fingerprint);
    if (materialChange || elapsedCheckpoint || !isRunningPhase(state.phase)) {
        m_lastElapsedPersists.insert(state.tableId, state.elapsedSeconds);
        if (shouldRebuildTableList) {
            shouldRebuildTableList[0] = materialChange;
        }
        return true;
    }

    if (shouldRebuildTableList) {
        shouldRebuildTableList[0] = false;
    }
    return false;
}

} // namespace amt

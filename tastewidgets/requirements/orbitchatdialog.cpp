/*
   Copyright (C) 2026 European Space Agency - <maxime.perrotin@esa.int>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this program. If not, see <https://www.gnu.org/licenses/lgpl-2.1.html>.
*/

#include "orbitchatdialog.h"

#include "orbitchatcontroller.h"
#include "orbitmarkdown.h"

#include "tqtw_m2sapi.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QSvgRenderer>
#include <QTemporaryDir>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <functional>

namespace requirement {

namespace {

//: The base size of the transcript, in points. Comfortably larger than the
//: Qt default so an answer is readable without adjusting anything.
constexpr int kBasePointSize = 13;
//: Ctrl+wheel steps between this and this, so the transcript never becomes
//: unreadable in either direction.
constexpr int kMinPointSize = 8;
constexpr int kMaxPointSize = 34;

//: The stylesheet and HTML head for the light transcript.
QString transcriptHead(int pointSize)
{
    return QStringLiteral(
            "<head><style>"
            "body { font-family: 'DejaVu Sans', Verdana, Arial, sans-serif; font-size: %1pt; color: #1f2328; }"
            "p { margin: 4px 0; }"
            "h1, h2, h3, h4, h5, h6 { color: #123a6b; margin: 10px 0 4px 0; }"
            "a { color: #0b63c5; }"
            "code { font-family: 'DejaVu Sans Mono', monospace; background: #eef1f5; color: #b3005f; "
            "padding: 1px 3px; }"
            "pre { font-family: 'DejaVu Sans Mono', monospace; background: #f2f4f8; border: 1px solid #d8dee6; "
            "padding: 8px; white-space: pre-wrap; }"
            "pre code { background: transparent; color: #1f2328; padding: 0; }"
            "blockquote { border-left: 3px solid #c9d3df; margin: 4px 0; padding-left: 10px; color: #57606a; }"
            "table { border-collapse: collapse; margin: 6px 0; }"
            "th, td { border: 1px solid #d8dee6; padding: 4px 8px; }"
            "th { background: #eef1f5; }"
            "hr { border: none; border-top: 1px solid #d8dee6; }"
            ".note { color: #6a737d; }"
            ".you { color: #0b63c5; font-weight: bold; }"
            ".error { color: #b3261e; }"
            "</style></head>")
            .arg(pointSize);
}

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

//: A transcript that zooms on Ctrl+wheel and reports the step to the dialog.
class OrbitTranscript : public QTextBrowser
{
public:
    explicit OrbitTranscript(QWidget *parent = nullptr)
        : QTextBrowser(parent)
    {
    }

    std::function<void(int)> zoomStep;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            const int steps = event->angleDelta().y() > 0 ? 1 : -1;
            if (zoomStep) {
                zoomStep(steps);
            }
            event->accept();
            return;
        }
        QTextBrowser::wheelEvent(event);
    }
};

/*!
 * Puts the model's questions to the person as a form and returns one answer per
 * question, or an empty list when the form is declined.
 */
QList<tqtw_orbitacp::Answer> askForm(QWidget *parent, const QList<tqtw_orbitacp::Question> &questions)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Orbit asks"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    layout->addLayout(form);

    struct Row {
        const tqtw_orbitacp::Question *question = nullptr;
        QComboBox *combo = nullptr;
        QListWidget *list = nullptr;
        QLineEdit *typed = nullptr;
    };
    QList<Row> rows;
    for (const tqtw_orbitacp::Question &q : questions) {
        auto *column = new QVBoxLayout();
        if (!q.text.isEmpty()) {
            column->addWidget(new QLabel(escaped(q.text), &dialog));
        }
        Row row;
        row.question = &q;
        if (!q.choices.isEmpty()) {
            if (q.many) {
                row.list = new QListWidget(&dialog);
                for (const tqtw_orbitacp::QuestionChoice &c : q.choices) {
                    auto *item = new QListWidgetItem(
                            c.text + (c.note.isEmpty() ? QString() : QStringLiteral(" — ") + c.note));
                    item->setData(Qt::UserRole, c.text);
                    row.list->addItem(item);
                }
                column->addWidget(row.list);
            } else {
                row.combo = new QComboBox(&dialog);
                for (const tqtw_orbitacp::QuestionChoice &c : q.choices) {
                    row.combo->addItem(
                            c.text + (c.note.isEmpty() ? QString() : QStringLiteral(" — ") + c.note), c.text);
                }
                column->addWidget(row.combo);
            }
        }
        if (q.freeText) {
            row.typed = new QLineEdit(&dialog);
            row.typed->setPlaceholderText(
                    q.choices.isEmpty() ? QObject::tr("Type an answer") : QObject::tr("Or type an answer"));
            column->addWidget(row.typed);
        }
        form->addRow(!q.topic.isEmpty() ? escaped(q.topic) : QString(), column);
        rows.append(row);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }

    QList<tqtw_orbitacp::Answer> answers;
    for (const Row &row : rows) {
        tqtw_orbitacp::Answer answer;
        if (row.combo != nullptr) {
            answer.picked.append(row.combo->currentData().toString());
        } else if (row.list != nullptr) {
            for (QListWidgetItem *item : row.list->selectedItems()) {
                answer.picked.append(item->data(Qt::UserRole).toString());
            }
        }
        if (row.typed != nullptr) {
            answer.typed = row.typed->text();
        }
        answers.append(answer);
    }
    return answers;
}

} // namespace

OrbitChatDialog::OrbitChatDialog(const QString &projectDir, QWidget *parent)
    : QDialog(parent)
    , m_controller(new OrbitChatController(this))
    , m_projectDir(projectDir)
{
    setWindowTitle(tr("Orbit — requirements assistant"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(820, 720);

    auto *transcript = new OrbitTranscript(this);
    m_transcript = transcript;
    m_transcript->setOpenExternalLinks(true);
    m_transcript->setStyleSheet(QStringLiteral(
            "QTextBrowser { background: #fbfbfd; color: #1f2328; border: 1px solid #d8dee6; }"));
    transcript->zoomStep = [this](int steps) {
        m_pointSize = qBound(kMinPointSize, m_pointSize + steps, kMaxPointSize);
        renderTranscript();
    };
    m_pointSize = kBasePointSize;

    m_entry = new QLineEdit(this);
    m_entry->setPlaceholderText(tr("Ask Orbit about the requirements… (starting)"));
    m_entry->setEnabled(false);
    connect(m_entry, &QLineEdit::returnPressed, this, &OrbitChatDialog::send);

    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, m_controller, &OrbitChatController::cancel);

    m_status = new QLabel(tr("Starting orbit…"), this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_transcript, 1);
    layout->addWidget(m_status);
    // Helper buttons live in their own row, above the entry line.
    m_helperRow = new QHBoxLayout();
    m_helperRow->addStretch(1);
    layout->addLayout(m_helperRow);
    auto *row = new QHBoxLayout();
    row->addWidget(m_entry, 1);
    row->addWidget(m_stopButton);
    layout->addLayout(row);

    connect(m_controller, &OrbitChatController::ready, this, &OrbitChatDialog::onReady);
    connect(m_controller, &OrbitChatController::failed, this, &OrbitChatDialog::onFailed);
    connect(m_controller, &OrbitChatController::eventReceived, this, &OrbitChatDialog::onEvent);
    connect(m_controller, &OrbitChatController::turnDone, this, &OrbitChatDialog::onTurnDone);
    connect(m_controller, &OrbitChatController::permissionRequested, this, &OrbitChatDialog::onPermissionRequested);
    connect(m_controller, &OrbitChatController::questionsRequested, this, &OrbitChatDialog::onQuestionsRequested);

    appendNote(tr("<i>Working in %1</i>").arg(escaped(m_projectDir)));
    // The agent is started in showEvent(), once the briefing (set by the caller)
    // is known - so the caller can setBriefing() and addHelper() first.
}

OrbitChatDialog::~OrbitChatDialog()
{
    m_controller->shutdown();
}

void OrbitChatDialog::onReady()
{
    m_entry->setEnabled(true);
    m_entry->setPlaceholderText(tr("Ask Orbit to build or connect functions…"));
    m_status->setText(tr("Ready. Ctrl+wheel to zoom."));
}

void OrbitChatDialog::onFailed(const QString &message)
{
    appendNote(QStringLiteral("<span class=\"error\">%1</span>").arg(escaped(message)));
    setThinking(false);
    if (!m_entry->isEnabled()) {
        m_status->setText(tr("Orbit is not available: %1").arg(message));
    }
}

void OrbitChatDialog::onEvent(const tqtw_orbitacp::Event &event)
{
    using K = tqtw_orbitacp::Event::Kind;
    switch (event.kind) {
    case K::Text:
        appendAnswerChunk(event.text);
        break;
    case K::Thought:
        // Reasoning is kept quiet to keep the transcript readable.
        break;
    case K::Tool: {
        const QString label = tr("[tool] %1 (%2)").arg(event.title, event.status);
        appendNote(escaped(label));
        break;
    }
    case K::Task: {
        appendNote(escaped(tr("[agent] %1 — %2").arg(event.title.isEmpty() ? event.id : event.title, event.status)));
        break;
    }
    case K::Plan: {
        for (const tqtw_orbitacp::PlanStep &step : event.steps) {
            appendNote(escaped(tr("[plan] %1 (%2)").arg(step.title, step.status)));
        }
        break;
    }
    default:
        break;
    }
}

void OrbitChatDialog::onTurnDone(const QString &stoppedBecause)
{
    if (!stoppedBecause.isEmpty() && stoppedBecause != QLatin1String("done")) {
        appendNote(escaped(tr("(%1)").arg(stoppedBecause)));
    }
    if (m_dirtyAnswer) {
        renderTranscript();
        m_dirtyAnswer = false;
    }
    setThinking(false);
}

void OrbitChatDialog::onPermissionRequested(const tqtw_orbitacp::PermissionRequest &request)
{
    QString text = request.title;
    if (!request.command().isEmpty()) {
        text += QStringLiteral("\n\n") + request.command();
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Allow Orbit to proceed?"), text, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    m_controller->answerPermission(
            answer == QMessageBox::Yes ? QString::fromLatin1(tqtw_orbitacp::ONCE) : QString::fromLatin1(tqtw_orbitacp::REJECT));
}

void OrbitChatDialog::onQuestionsRequested(const QList<tqtw_orbitacp::Question> &questions)
{
    m_controller->answerQuestions(askForm(this, questions));
}

void OrbitChatDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (!m_started) {
        m_started = true;
        m_controller->start(m_projectDir, m_briefing);
    }
}

void OrbitChatDialog::setBriefing(const QString &briefing)
{
    m_briefing = briefing;
}

void OrbitChatDialog::addHelper(const QString &label, const QString &prompt)
{
    if (m_helperRow == nullptr) {
        return;
    }
    auto *button = new QPushButton(label, this);
    button->setToolTip(prompt);
    connect(button, &QPushButton::clicked, this, [this, prompt]() { askQuestion(prompt); });
    // Before the trailing stretch, so the buttons sit on the right.
    m_helperRow->insertWidget(m_helperRow->count() - 1, button);
}

void OrbitChatDialog::send()
{
    const QString question = m_entry->text().trimmed();
    if (question.isEmpty()) {
        return;
    }
    m_entry->clear();
    askQuestion(question);
}

void OrbitChatDialog::askQuestion(const QString &question)
{
    // A helper button can be pressed while orbit is still starting, or while an
    // answer streams; both would be refused (or interleaved), so ignore it.
    if (question.trimmed().isEmpty() || !m_entry->isEnabled()) {
        return;
    }
    appendNote(QStringLiteral("<span class=\"you\">you:</span> %1").arg(escaped(question)));

    Block answer;
    answer.markdown = true;
    answer.text = QStringLiteral("*Orbit is thinking…*");
    m_answerMarkdown.clear();
    m_answerIndex = m_blocks.size();
    m_blocks.append(answer);
    m_dirtyAnswer = true;
    renderTranscript();
    setThinking(true);
    m_controller->ask(question);
}

void OrbitChatDialog::appendAnswerChunk(const QString &text)
{
    if (text.isEmpty()) {
        return;
    }
    m_answerMarkdown += text;
    if (m_answerIndex >= 0 && m_answerIndex < m_blocks.size()) {
        m_blocks[m_answerIndex].text = m_answerMarkdown;
    } else {
        Block answer;
        answer.markdown = true;
        answer.text = m_answerMarkdown;
        m_answerIndex = m_blocks.size();
        m_blocks.append(answer);
    }
    m_dirtyAnswer = true;
    renderTranscript();
}

void OrbitChatDialog::appendNote(const QString &html)
{
    Block note;
    note.markdown = false;
    note.text = html;
    m_blocks.append(note);
    renderTranscript();
}

void OrbitChatDialog::renderTranscript()
{
    // Keep the view pinned to the bottom when it already is, so a live answer
    // does not yank the scroll position away from someone reading above.
    QScrollBar *bar = m_transcript->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;

    QString body;
    for (const Block &block : m_blocks) {
        if (block.markdown) {
            body += answerToHtml(block.text);
        } else {
            body += QStringLiteral("<div class=\"note\">%1</div>").arg(block.text);
        }
    }
    m_transcript->setHtml(transcriptHead(m_pointSize) + QStringLiteral("<body>") + body + QStringLiteral("</body>"));

    if (atBottom) {
        bar->setValue(bar->maximum());
    }
}

void OrbitChatDialog::setThinking(bool thinking)
{
    m_entry->setEnabled(!thinking && m_status->text() != tr("Starting orbit…"));
    m_stopButton->setEnabled(thinking);
    m_status->setText(thinking ? tr("Orbit is working…") : tr("Ready. Ctrl+wheel to zoom."));
}

QString OrbitChatDialog::answerToHtml(const QString &markdown) const
{
    const QStringList sources = OrbitMarkdown::mermaidBlocks(markdown);
    if (sources.isEmpty()) {
        return OrbitMarkdown::toHtml(markdown);
    }
    QStringList placeholders;
    for (const QString &source : sources) {
        placeholders.append(mermaidImage(source));
    }
    return OrbitMarkdown::toHtml(markdown, placeholders);
}

QString OrbitChatDialog::mermaidImage(const QString &source) const
{
    using namespace tqtw_mermaid2svg;
    try {
        tqtw_mermaid2svg::Options options;
        options.theme = QStringLiteral("default").toStdString();
        options.backgroundColor = "transparent";
        const tqtw_mermaid2svg::Result result = tqtw_mermaid2svg::render(source.toStdString(), options);
        // Rasterize the SVG to a PNG and hand QTextDocument a data: URL, so the
        // image is self-contained and zooming scales it with the text.
        QSvgRenderer renderer(QByteArray::fromStdString(result.svg));
        if (!renderer.isValid()) {
            throw std::runtime_error("the SVG did not render");
        }
        const QSize size = renderer.defaultSize().scaled(900, 900, Qt::KeepAspectRatio);
        QImage image(size.isValid() && !size.isEmpty() ? size : QSize(600, 300), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        painter.end();
        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        const QString data = QString::fromLatin1(png.toBase64());
        return QStringLiteral("<p><img src=\"data:image/png;base64,%1\" width=\"%2\"/></p>")
                .arg(data)
                .arg(std::max(200, image.width()));
    } catch (const std::exception &e) {
        return QStringLiteral("<pre><code>%1</code></pre>")
                .arg(source.toHtmlEscaped());
    }
}

} // namespace requirement

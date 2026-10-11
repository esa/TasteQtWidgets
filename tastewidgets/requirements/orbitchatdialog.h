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

#pragma once

#include "tqtw_types.h"

#include <QDialog>
#include <QList>
#include <QVector>

class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QTextBrowser;
class QLabel;

namespace requirement {

class OrbitChatController;

/*!
 * The Orbit chatbot: a resizable chat window that talks to `orbit acp` about
 * the project the interface view lives in. It runs the conversation on a
 * controller of its own, so the window never blocks; a permission request or a
 * model question opens a dialog and the controller waits for the answer.
 */
class OrbitChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OrbitChatDialog(const QString &projectDir, QWidget *parent = nullptr);
    ~OrbitChatDialog() override;

    //: Text the agent is given before the user's first question - context and
    //: standing instructions. Sent once, when the window is first shown.
    void setBriefing(const QString &briefing);
    //: Add a helper button under the transcript that asks `prompt` when pressed.
    void addHelper(const QString &label, const QString &prompt);

protected:
    void showEvent(QShowEvent *event) override;

private Q_SLOTS:
    void onReady();
    void onFailed(const QString &message);
    void onEvent(const tqtw_orbitacp::Event &event);
    void onTurnDone(const QString &stoppedBecause);
    void onPermissionRequested(const tqtw_orbitacp::PermissionRequest &request);
    void onQuestionsRequested(const QList<tqtw_orbitacp::Question> &questions);

private:
    //: One piece of the transcript: an orbit answer (Markdown, rendered when
    //: it changes) or a note and a question already in HTML.
    struct Block {
        bool markdown = false;
        QString text;
    };

    void send();
    //: Sends `question` as a user turn, whatever its source (the entry line or
    //: a helper button), and starts a fresh answer block. Ignored when busy.
    void askQuestion(const QString &question);
    void appendAnswerChunk(const QString &text);
    void appendNote(const QString &html);
    void renderTranscript();
    //: Renders a Markdown answer to HTML, turning ```mermaid fences into
    //: inline SVG images (rasterized to PNG resources the document can hold).
    QString answerToHtml(const QString &markdown) const;
    //: The <img ...> a mermaid diagram becomes, or the source in a <pre> when
    //: it cannot be rendered.
    QString mermaidImage(const QString &source) const;
    QString m_resourcePrefix;

    void setThinking(bool thinking);

    OrbitChatController *m_controller = nullptr;
    QTextBrowser *m_transcript = nullptr;
    QLineEdit *m_entry = nullptr;
    QPushButton *m_stopButton = nullptr;
    QLabel *m_status = nullptr;
    QHBoxLayout *m_helperRow = nullptr;
    QString m_projectDir;
    QString m_briefing;
    bool m_started = false;
    QVector<Block> m_blocks;
    QString m_answerMarkdown;
    int m_answerIndex = -1;
    int m_pointSize = 13;
    bool m_dirtyAnswer = false;
};

} // namespace requirement

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

#include <QString>
#include <QStringList>

namespace requirement {

/*!
 * A small Markdown-to-HTML renderer for the Orbit chat transcript.
 *
 * Orbit answers in Markdown, so the chat shows its answers formatted rather
 * than as plain text. This covers the subset that appears in practice -
 * headings, paragraphs, emphasis, inline code, fenced code blocks, links,
 * bullet and numbered lists, block quotes, horizontal rules and tables - and
 * nothing more. It is deliberately tolerant: text is rendered as it streams
 * in, so anything half-written is shown as it is and finishes correctly on
 * the next chunk.
 *
 * The HTML it emits is the restricted subset QTextDocument understands, so it
 * can be handed straight to a QTextBrowser.
 */
class OrbitMarkdown
{
public:
    //: Renders Markdown to an HTML fragment for a QTextBrowser.
    static QString toHtml(const QString &markdown);
    //: The same, with one HTML replacement per `mermaidBlocks` entry (an
    //: <img> for the diagram, say), so the fences become pictures.
    static QString toHtml(const QString &markdown, const QStringList &mermaidPlaceholders);
    //: Renders one line of text with only the inline rules applied.
    static QString inlineToHtml(const QString &text);

    //: The source of every ```mermaid fence in the document, in order.
    static QStringList mermaidBlocks(const QString &markdown);

private:
    explicit OrbitMarkdown(const QString &markdown, const QStringList &placeholders);
    QString render();
    QString renderFenced(const QString &language, const QStringList &body, int *mermaidIndex);

    QStringList m_lines;
    QStringList m_placeholders;
    int m_nextMermaid = 0;
};

} // namespace requirement

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

#include "orbitmarkdown.h"

#include <QRegularExpression>
#include <QStringList>
#include <QVector>

namespace requirement {

namespace {

//: Placeholders for inline code spans, so the other inline rules never look
//: inside them. They are ASCII control characters no Markdown text carries.
const QChar kCodeOpen(0x1);
const QChar kCodeClose(0x2);

bool isBlank(const QString &line)
{
    return line.trimmed().isEmpty();
}

//: The "# ", "## " … prefix of an ATX heading, or 0 when there is none.
int headingLevel(const QString &line)
{
    int level = 0;
    while (level < line.size() && line.at(level) == QLatin1Char('#')) {
        ++level;
    }
    if (level == 0 || level > 6) {
        return 0;
    }
    if (level < line.size() && line.at(level) != QLatin1Char(' ')) {
        return 0;
    }
    return level;
}

//: A fenced code block boundary: ``` or ~~~ of at least three, and its info
//: string (the language name).
bool fenceInfo(const QString &line, QString *fence, QString *info)
{
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"))) {
        const QChar marker = trimmed.at(0);
        int n = 0;
        while (n < trimmed.size() && trimmed.at(n) == marker) {
            ++n;
        }
        if (n >= 3) {
            *fence = trimmed.left(n);
            *info = trimmed.mid(n).trimmed();
            return true;
        }
    }
    return false;
}

//: True for a list item, with the ordered/unordered kind and the text.
bool listItem(const QString &line, bool *ordered, QString *text)
{
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1String("- ")) || trimmed.startsWith(QLatin1String("* "))
            || trimmed.startsWith(QLatin1String("+ "))) {
        *ordered = false;
        *text = trimmed.mid(2);
        return true;
    }
    static const QRegularExpression orderedRe(QStringLiteral("^(\\d+)\\.\\s+(.*)$"));
    const QRegularExpressionMatch match = orderedRe.match(trimmed);
    if (match.hasMatch()) {
        *ordered = true;
        *text = match.captured(2);
        return true;
    }
    return false;
}

//: True for a horizontal rule (---, ***, ___ on a line of their own).
bool isHorizontalRule(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.size() < 3) {
        return false;
    }
    const QChar c = trimmed.at(0);
    if (c != QLatin1Char('-') && c != QLatin1Char('*') && c != QLatin1Char('_')) {
        return false;
    }
    for (const QChar &ch : trimmed) {
        if (ch != c) {
            return false;
        }
    }
    return true;
}

//: The cells of a table row, without the leading and trailing pipe.
QStringList tableCells(const QString &line)
{
    QString trimmed = line.trimmed();
    if (trimmed.startsWith(QLatin1Char('|'))) {
        trimmed = trimmed.mid(1);
    }
    if (trimmed.endsWith(QLatin1Char('|'))) {
        trimmed.chop(1);
    }
    QStringList cells;
    for (const QString &cell : trimmed.split(QLatin1Char('|'))) {
        cells.append(cell.trimmed());
    }
    return cells;
}

//: The separator row of a table: |---|:--:|---|.
bool isTableSeparator(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.contains(QLatin1Char('-'))) {
        return false;
    }
    const QStringList cells = tableCells(line);
    if (cells.isEmpty()) {
        return false;
    }
    static const QRegularExpression cellRe(QStringLiteral("^:?-{1,}:?$"));
    for (const QString &cell : cells) {
        if (!cellRe.match(cell).hasMatch()) {
            return false;
        }
    }
    return true;
}

} // namespace

QString OrbitMarkdown::inlineToHtml(const QString &text)
{
    QString work = text.toHtmlEscaped();

    // Inline code first, parked behind placeholders so no other rule touches
    // what is inside it.
    QVector<QString> code;
    static const QRegularExpression codeRe(QStringLiteral("`([^`]+)`"));
    QString parked;
    int last = 0;
    QRegularExpressionMatchIterator it = codeRe.globalMatch(work);
    QVector<QRegularExpressionMatch> matches;
    while (it.hasNext()) {
        matches.append(it.next());
    }
    for (const QRegularExpressionMatch &match : matches) {
        parked += work.mid(last, match.capturedStart() - last);
        parked += kCodeOpen;
        parked += QString::number(code.size());
        parked += kCodeClose;
        code.append(match.captured(1));
        last = match.capturedEnd();
    }
    parked += work.mid(last);
    work = parked;

    // Links: [text](url).
    static const QRegularExpression linkRe(QStringLiteral("\\[([^\\]]+)\\]\\(([^)\\s]+)\\)"));
    work.replace(linkRe, QStringLiteral("<a href=\"\\2\">\\1</a>"));

    // Emphasis, bold before italic so ** wins over *.
    static const QRegularExpression boldRe(QStringLiteral("\\*\\*(.+?)\\*\\*"));
    work.replace(boldRe, QStringLiteral("<b>\\1</b>"));
    static const QRegularExpression boldUnderscoreRe(QStringLiteral("__(.+?)__"));
    work.replace(boldUnderscoreRe, QStringLiteral("<b>\\1</b>"));
    static const QRegularExpression strikeRe(QStringLiteral("~~(.+?)~~"));
    work.replace(strikeRe, QStringLiteral("<s>\\1</s>"));
    static const QRegularExpression italicStarRe(QStringLiteral("\\*([^*]+?)\\*"));
    work.replace(italicStarRe, QStringLiteral("<i>\\1</i>"));
    static const QRegularExpression italicUnderscoreRe(QStringLiteral("(?<![\\w])_([^_]+?)_(?![\\w])"));
    work.replace(italicUnderscoreRe, QStringLiteral("<i>\\1</i>"));

    // Bring the code spans back.
    for (int i = 0; i < code.size(); ++i) {
        const QString token = QString(kCodeOpen) + QString::number(i) + QString(kCodeClose);
        work.replace(token, QStringLiteral("<code>%1</code>").arg(code.at(i)));
    }
    return work;
}

OrbitMarkdown::OrbitMarkdown(const QString &markdown, const QStringList &placeholders)
    : m_lines(markdown.split(QLatin1Char('\n')))
    , m_placeholders(placeholders)
{
}

QString OrbitMarkdown::render()
{
    QString html;
    int i = 0;
    const int n = m_lines.size();
    while (i < n) {
        const QString &line = m_lines.at(i);
        QString fence;
        QString info;

        if (isBlank(line)) {
            ++i;
            continue;
        }

        // Fenced code block.
        if (fenceInfo(line, &fence, &info)) {
            QStringList body;
            ++i;
            while (i < n && !m_lines.at(i).trimmed().startsWith(fence)) {
                body.append(m_lines.at(i));
                ++i;
            }
            if (i < n) {
                ++i; // the closing fence
            }
            const QString language = info.isEmpty() ? QString()
                                                    : QStringLiteral(" class=\"language-%1\"").arg(info.toHtmlEscaped());
            if (info.compare(QLatin1String("mermaid"), Qt::CaseInsensitive) == 0) {
                if (m_nextMermaid < m_placeholders.size()) {
                    html += m_placeholders.at(m_nextMermaid);
                } else {
                    html += QStringLiteral("<pre><code%1>%2</code></pre>")
                                    .arg(language, body.join(QLatin1Char('\n')).toHtmlEscaped());
                }
                ++m_nextMermaid;
            } else {
                html += QStringLiteral("<pre><code%1>%2</code></pre>").arg(language, body.join(QLatin1Char('\n')).toHtmlEscaped());
            }
            continue;
        }

        // Heading.
        const int level = headingLevel(line);
        if (level > 0) {
            const QString text = line.mid(level).trimmed();
            html += QStringLiteral("<h%1>%2</h%1>").arg(level).arg(inlineToHtml(text));
            ++i;
            continue;
        }

        // Horizontal rule.
        if (isHorizontalRule(line) && line.trimmed().size() >= 3) {
            html += QStringLiteral("<hr/>");
            ++i;
            continue;
        }

        // Block quote: collect the run of "> " lines.
        if (line.trimmed().startsWith(QLatin1Char('>'))) {
            QStringList quoted;
            while (i < n && m_lines.at(i).trimmed().startsWith(QLatin1Char('>'))) {
                QString inner = m_lines.at(i).trimmed();
                inner = inner.mid(1);
                if (inner.startsWith(QLatin1Char(' '))) {
                    inner = inner.mid(1);
                }
                quoted.append(inner);
                ++i;
            }
            html += QStringLiteral("<blockquote>%1</blockquote>").arg(OrbitMarkdown::toHtml(quoted.join(QLatin1Char('\n'))));
            continue;
        }

        // Table: a header row, a separator row, then body rows.
        if (line.contains(QLatin1Char('|')) && i + 1 < n && isTableSeparator(m_lines.at(i + 1))) {
            const QStringList header = tableCells(line);
            html += QStringLiteral("<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\">");
            html += QStringLiteral("<thead><tr>");
            for (const QString &cell : header) {
                html += QStringLiteral("<th>%1</th>").arg(inlineToHtml(cell));
            }
            html += QStringLiteral("</tr></thead><tbody>");
            i += 2;
            while (i < n && m_lines.at(i).contains(QLatin1Char('|')) && !isBlank(m_lines.at(i))) {
                html += QStringLiteral("<tr>");
                for (const QString &cell : tableCells(m_lines.at(i))) {
                    html += QStringLiteral("<td>%1</td>").arg(inlineToHtml(cell));
                }
                html += QStringLiteral("</tr>");
                ++i;
            }
            html += QStringLiteral("</tbody></table>");
            continue;
        }

        // List: a run of items of the same kind.
        bool ordered = false;
        QString itemText;
        if (listItem(line, &ordered, &itemText)) {
            const QString tag = ordered ? QStringLiteral("ol") : QStringLiteral("ul");
            html += QStringLiteral("<%1>").arg(tag);
            while (i < n) {
                bool itemOrdered = false;
                QString text;
                if (!listItem(m_lines.at(i), &itemOrdered, &text) || itemOrdered != ordered) {
                    break;
                }
                html += QStringLiteral("<li>%1</li>").arg(inlineToHtml(text));
                ++i;
            }
            html += QStringLiteral("</%1>").arg(tag);
            continue;
        }

        // Paragraph: consecutive plain lines.
        QStringList paragraph;
        while (i < n && !isBlank(m_lines.at(i))) {
            const QString &current = m_lines.at(i);
            QString fenceCheck;
            QString infoCheck;
            if (headingLevel(current) > 0 || fenceInfo(current, &fenceCheck, &infoCheck)
                    || current.trimmed().startsWith(QLatin1Char('>'))) {
                break;
            }
            bool listOrdered = false;
            QString listText;
            if (listItem(current, &listOrdered, &listText)) {
                break;
            }
            paragraph.append(current.trimmed());
            ++i;
        }
        if (!paragraph.isEmpty()) {
            html += QStringLiteral("<p>%1</p>").arg(inlineToHtml(paragraph.join(QLatin1Char(' '))));
        } else {
            ++i; // a line the loop refused but that is not a block on its own
        }
    }
    return html;
}

QString OrbitMarkdown::toHtml(const QString &markdown)
{
    OrbitMarkdown renderer(markdown, {});
    return renderer.render();
}

QString OrbitMarkdown::toHtml(const QString &markdown, const QStringList &mermaidPlaceholders)
{
    OrbitMarkdown renderer(markdown, mermaidPlaceholders);
    return renderer.render();
}

QStringList OrbitMarkdown::mermaidBlocks(const QString &markdown)
{
    QStringList blocks;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    int i = 0;
    while (i < lines.size()) {
        QString fence;
        QString info;
        if (fenceInfo(lines.at(i), &fence, &info) && info.compare(QLatin1String("mermaid"), Qt::CaseInsensitive) == 0) {
            QStringList body;
            ++i;
            while (i < lines.size() && !lines.at(i).trimmed().startsWith(fence)) {
                body.append(lines.at(i));
                ++i;
            }
            if (i < lines.size()) {
                ++i;
            }
            blocks.append(body.join(QLatin1Char('\n')));
        } else {
            ++i;
        }
    }
    return blocks;
}

} // namespace requirement

#pragma once

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QVector>

struct Work {
    QString id;
    QString title;
    QString author;
    QString authorUrl;
    QString fandom;
    QString rating;
    QString warnings;
    QString category;
    QString summary;
    int chaptersWritten = 0;
    int chaptersTotal = 0;
    int wordCount = 0;
    int commentCount = 0;
    int kudosCount = 0;
    int hitCount = 0;
    QString language;
    QDateTime published;
    QDateTime updated;
    bool complete = false;
    QStringList tags;
    QStringList relationships;
    QStringList characters;
    QString bookmarksUrl;
    QString commentsUrl;
};

struct SearchResult {
    QVector<Work> works;
    int totalResults = 0;
    int currentPage = 1;
    int totalPages = 1;
    QString errorMessage;
};

struct UserInfo {
    QString username;
    QString dashboardUrl;
    bool isLoggedIn = false;
};

struct WorkChapter {
    QString title;
    QString text;
    QString notesBefore = {};
    QString notesAfter = {};
    QString exportText() const {
        QStringList sections{title};
        if (!notesBefore.isEmpty()) sections << "Author notes\n\n" + notesBefore;
        sections << text;
        if (!notesAfter.isEmpty()) sections << "End notes\n\n" + notesAfter;
        return sections.join("\n\n");
    }
};

struct AuthorProfile {
    QString username;
    QString title;
    QString details;
    QString biography;
    QString error;
};

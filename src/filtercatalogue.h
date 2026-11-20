#pragma once
#include "searchfilters.h"

inline SearchOptions standardFilterChoices()
{
    SearchOptions result;
    result.choices[QStringLiteral("rating_ids")] = {
        {QStringLiteral("Not Rated"), "9"}, {QStringLiteral("General Audiences"), "10"},
        {QStringLiteral("Teen And Up Audiences"), "11"}, {QStringLiteral("Mature"), "12"}, {QStringLiteral("Explicit"), "13"}};
    result.choices[QStringLiteral("archive_warning_ids[]")] = {
        {QStringLiteral("Creator Chose Not To Use Archive Warnings"), "14"},
        {QStringLiteral("No Archive Warnings Apply"), "16"},
        {QStringLiteral("Graphic Depictions Of Violence"), "17"},
        {QStringLiteral("Major Character Death"), "18"}, {QStringLiteral("Rape/Non-Con"), "19"},
        {QStringLiteral("Underage Sex"), "20"}};
    result.choices[QStringLiteral("category_ids[]")] = {
        {QStringLiteral("F/F"), "116"}, {QStringLiteral("F/M"), "22"}, {QStringLiteral("Gen"), "21"},
        {QStringLiteral("M/M"), "23"}, {QStringLiteral("Multi"), "2246"}, {QStringLiteral("Other"), "24"}};
    result.choices[QStringLiteral("language_id")] = {
        {QStringLiteral("English"), "en"}, {QStringLiteral("Eesti"), "et"}, {QStringLiteral("Français"), "fr"},
        {QStringLiteral("Deutsch"), "de"}, {QStringLiteral("Español"), "es"}, {QStringLiteral("Italiano"), "it"},
        {QStringLiteral("Português"), "pt"}, {QStringLiteral("Русский"), "ru"}, {QStringLiteral("Українська"), "uk"},
        {QStringLiteral("Polski"), "pl"}, {QStringLiteral("Čeština"), "cs"}, {QStringLiteral("Slovenčina"), "sk"},
        {QStringLiteral("Slovenščina"), "sl"}, {QStringLiteral("Nederlands"), "nl"}, {QStringLiteral("Suomi"), "fi"},
        {QStringLiteral("Svenska"), "sv"}, {QStringLiteral("Dansk"), "da"}, {QStringLiteral("Norsk"), "no"},
        {QStringLiteral("Íslenska"), "is"}, {QStringLiteral("Latviešu"), "lv"}, {QStringLiteral("Lietuvių"), "lt"},
        {QStringLiteral("Magyar"), "hu"}, {QStringLiteral("Română"), "ro"}, {QStringLiteral("Български"), "bg"},
        {QStringLiteral("Српски"), "sr"}, {QStringLiteral("Hrvatski"), "hr"}, {QStringLiteral("Ελληνικά"), "el"},
        {QStringLiteral("Türkçe"), "tr"}, {QStringLiteral("العربية"), "ar"}, {QStringLiteral("עברית"), "he"},
        {QStringLiteral("فارسی"), "fa"}, {QStringLiteral("हिन्दी"), "hi"}, {QStringLiteral("বাংলা"), "bn"},
        {QStringLiteral("Bahasa Indonesia"), "id"}, {QStringLiteral("Bahasa Melayu"), "ms"},
        {QStringLiteral("Tiếng Việt"), "vi"}, {QStringLiteral("ไทย"), "th"}, {QStringLiteral("Filipino"), "fil"},
        {QStringLiteral("中文-普通话 國語"), "zh"}, {QStringLiteral("日本語"), "ja"}, {QStringLiteral("한국어"), "ko"},
        {QStringLiteral("Català"), "ca"}, {QStringLiteral("Esperanto"), "eo"}, {QStringLiteral("Latin"), "la"}};
    return result;
}

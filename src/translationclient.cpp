#include "translationclient.h"
#include "serviceurl.h"
#include <QJsonDocument>
#include <QJsonObject>

TranslationClient::TranslationClient(QObject *parent) : QObject(parent), m_manager(new QNetworkAccessManager(this)) {}
void TranslationClient::cancel()
{
    ++m_generation;
    if (m_reply) m_reply->abort();
    m_reply.clear(); m_chunks.clear(); m_output.clear(); m_key.clear();
}
void TranslationClient::translate(const QString &text, const QString &target, const QUrl &endpoint, const QString &key)
{
    cancel();
    if (!validTranslationService(endpoint)) {
        emit failed(QStringLiteral("Set a valid LibreTranslate service URL in Reading settings.")); return;
    }
    m_target = target; m_endpoint = endpoint; m_key = key;
    if (m_endpoint.path().isEmpty() || m_endpoint.path() == QStringLiteral("/")) m_endpoint.setPath(QStringLiteral("/translate"));
    int start = 0;
    while (start < text.size()) {
        int length = qMin(2000, int(text.size()) - start);
        if (start + length < text.size()) {
            const int space = text.lastIndexOf(QChar(' '), start + length - 1);
            const int line = text.lastIndexOf(QChar('\n'), start + length - 1);
            const int split = qMax(space, line);
            if (split > start + 1000) length = split - start + 1;
            else if (text[start + length - 1].isHighSurrogate()) --length;
        }
        m_chunks.append(text.mid(start, length)); start += length;
    }
    if (m_chunks.isEmpty()) { emit failed(QStringLiteral("No chapter text to translate.")); return; }
    emit progress(0, m_chunks.size());
    next(m_generation);
}
void TranslationClient::next(quint64 generation)
{
    if (generation != m_generation) return;
    if (m_output.size() == m_chunks.size()) { const QString text = m_output.join(QStringLiteral("\n")); m_key.clear(); emit completed(text); return; }
    QJsonObject body{{"q", m_chunks[m_output.size()]}, {"source", "auto"}, {"target", m_target}, {"format", "text"}};
    if (!m_key.isEmpty()) body.insert(QStringLiteral("api_key"), m_key);
    QNetworkRequest request(m_endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(45000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    m_reply = m_manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    auto *reply = m_reply.data();
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_generation) return;
        m_reply.clear();
        const auto object = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError || !object.value(QStringLiteral("translatedText")).isString()) {
            QString reason = object.value(QStringLiteral("error")).toString();
            if (reason.isEmpty()) reason = reply->error() == QNetworkReply::NoError ? QStringLiteral("Unexpected translation response") : reply->errorString();
            m_key.clear(); emit failed(QStringLiteral("Translation failed: %1").arg(reason)); return;
        }
        m_output.append(object.value(QStringLiteral("translatedText")).toString());
        emit progress(m_output.size(), m_chunks.size());
        next(generation);
    });
}

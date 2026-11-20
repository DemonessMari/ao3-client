#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>

class TranslationClient : public QObject {
    Q_OBJECT
    friend class FunctionalTest;
public:
    explicit TranslationClient(QObject *parent = nullptr);
    void translate(const QString &text, const QString &target, const QUrl &endpoint, const QString &key);
    void cancel();
signals:
    void completed(const QString &text);
    void failed(const QString &message);
    void progress(int finished, int total);
private:
    void next(quint64 generation);
    QNetworkAccessManager *m_manager;
    QPointer<QNetworkReply> m_reply;
    QStringList m_chunks, m_output;
    QString m_target, m_key;
    QUrl m_endpoint;
    quint64 m_generation = 0;
};

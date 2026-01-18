#include "RBCEditorRuntime/infra/network/HttpClient.h"

namespace rbc {

HttpClient::HttpClient(QObject *parent) : QObject(parent) {}
HttpClient::~HttpClient() {}
void HttpClient::setServerUrl(const QString &url) {
    m_serverUrl = url;
}

}// namespace rbc
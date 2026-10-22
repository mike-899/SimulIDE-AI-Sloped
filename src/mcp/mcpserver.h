/***************************************************************************
 *   Embedded MCP (Model Context Protocol) server for SimulIDE             *
 *   Transport: MCP "Streamable HTTP" (JSON responses only) on localhost.  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef MCPSERVER_H
#define MCPSERVER_H

#include <QObject>
#include <QHash>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>

class QTcpServer;
class QTcpSocket;
class McpTools;

class McpServer : public QObject
{
    Q_OBJECT
    public:
        explicit McpServer( QObject* parent = 0l );
        ~McpServer();

        // Reads settings ("mcp/enabled", "mcp/port") and CLI args
        // (--mcp-port N, --no-mcp) and starts listening if enabled.
        static McpServer* startDefault( QObject* parent );

        bool listen( quint16 port );
        quint16 port() const;

    private slots:
        void onNewConnection();
        void onReadyRead();
        void onDisconnected();

    private:
        void processBuffer( QTcpSocket* sock );
        void sendResponse( QTcpSocket* sock, int status, const QByteArray& body,
                           const QByteArray& contentType = "application/json",
                           bool keepAlive = true );
        QJsonValue handleMessage( const QJsonObject& msg );   // Null if notification
        QJsonObject makeError( const QJsonValue& id, int code, const QString& msg );
        static bool originAllowed( const QByteArray& origin );

        QTcpServer* m_server;
        McpTools*   m_tools;
        QHash<QTcpSocket*, QByteArray> m_buffers;
};

#endif

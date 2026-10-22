/***************************************************************************
 *   Embedded MCP (Model Context Protocol) server for SimulIDE             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "mcpserver.h"
#include "mcptools.h"
#include "mainwindow.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QSettings>
#include <QUrl>
#include <QDebug>

static const char* kProtocolVersion = "2025-06-18";
static const int   kMaxBody = 32*1024*1024;

McpServer::McpServer( QObject* parent )
         : QObject( parent )
{
    m_server = new QTcpServer( this );
    m_tools  = new McpTools( this );
    connect( m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()) );
}
McpServer::~McpServer() {}

McpServer* McpServer::startDefault( QObject* parent )
{
    bool enabled = true;
    int  port    = 7531;

    QSettings* s = MainWindow::self() ? MainWindow::self()->settings() : 0l;
    if( s )
    {
        if( s->contains("mcp/enabled") ) enabled = s->value("mcp/enabled").toBool();
        if( s->contains("mcp/port") )    port    = s->value("mcp/port").toInt();
    }
    QStringList args = QCoreApplication::arguments();
    for( int i=1; i<args.size(); ++i )
    {
        QString a = args[i];
        if( a == "--no-mcp" ) enabled = false;
        else if( a == "--mcp-port" && i+1 < args.size() ) { port = args[++i].toInt(); enabled = true; }
        else if( a.startsWith("--mcp-port=") ) { port = a.mid(11).toInt(); enabled = true; }
    }
    if( !enabled || port <= 0 || port > 65535 ) return 0l;

    McpServer* srv = new McpServer( parent );
    if( !srv->listen( port ) )
    {
        qWarning() << "MCP: could not listen on 127.0.0.1:" << port;
        delete srv;
        return 0l;
    }
    qDebug() << "MCP server listening on http://127.0.0.1:" + QString::number(port) + "/mcp";
    return srv;
}

bool McpServer::listen( quint16 port )
{
    return m_server->listen( QHostAddress::LocalHost, port );
}

quint16 McpServer::port() const { return m_server->serverPort(); }

void McpServer::onNewConnection()
{
    while( m_server->hasPendingConnections() )
    {
        QTcpSocket* sock = m_server->nextPendingConnection();
        m_buffers.insert( sock, QByteArray() );
        connect( sock, SIGNAL(readyRead()),    this, SLOT(onReadyRead()) );
        connect( sock, SIGNAL(disconnected()), this, SLOT(onDisconnected()) );
    }
}

void McpServer::onDisconnected()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>( sender() );
    if( !sock ) return;
    m_buffers.remove( sock );
    sock->deleteLater();
}

void McpServer::onReadyRead()
{
    QTcpSocket* sock = qobject_cast<QTcpSocket*>( sender() );
    if( !sock ) return;
    m_buffers[sock].append( sock->readAll() );
    processBuffer( sock );
}

bool McpServer::originAllowed( const QByteArray& origin )
{
    if( origin.isEmpty() || origin == "null" ) return origin.isEmpty();
    QUrl url( QString::fromUtf8( origin ) );
    QString host = url.host().toLower();
    return host == "localhost" || host == "127.0.0.1" || host == "::1" || host == "[::1]";
}

void McpServer::processBuffer( QTcpSocket* sock )
{
    for( ;; )
    {
        QByteArray& buf = m_buffers[sock];
        int hdrEnd = buf.indexOf( "\r\n\r\n" );
        if( hdrEnd < 0 )
        {
            if( buf.size() > 64*1024 ) { sendResponse( sock, 431, "", "text/plain", false ); }
            return;
        }
        QList<QByteArray> lines = buf.left( hdrEnd ).split( '\n' );
        QList<QByteArray> reqLine = lines.value(0).trimmed().split( ' ' );
        if( reqLine.size() < 3 ) { sendResponse( sock, 400, "", "text/plain", false ); return; }

        QByteArray method  = reqLine[0];
        QByteArray path    = reqLine[1];
        QByteArray version = reqLine[2];
        int qm = path.indexOf('?');
        if( qm >= 0 ) path = path.left( qm );

        QHash<QByteArray, QByteArray> headers;
        for( int i=1; i<lines.size(); ++i )
        {
            QByteArray l = lines[i].trimmed();
            int c = l.indexOf(':');
            if( c <= 0 ) continue;
            headers.insert( l.left(c).trimmed().toLower(), l.mid(c+1).trimmed() );
        }
        bool okLen = true;
        int contentLength = headers.value( "content-length", "0" ).toInt( &okLen );
        if( !okLen || contentLength < 0 || contentLength > kMaxBody )
        { sendResponse( sock, 413, "", "text/plain", false ); return; }

        if( buf.size() < hdrEnd+4+contentLength ) return;      // Wait for body

        QByteArray body = buf.mid( hdrEnd+4, contentLength );
        buf.remove( 0, hdrEnd+4+contentLength );

        QByteArray conn = headers.value( "connection" ).toLower();
        bool keepAlive = ( version == "HTTP/1.1" ) ? ( conn != "close" ) : ( conn == "keep-alive" );

        if( !originAllowed( headers.value("origin") ) )
        {
            sendResponse( sock, 403, "{\"error\":\"Origin not allowed\"}", "application/json", keepAlive );
        }
        else if( path != "/mcp" && path != "/" )
        {
            sendResponse( sock, 404, "", "text/plain", keepAlive );
        }
        else if( method == "OPTIONS" )
        {
            sendResponse( sock, 204, "", "text/plain", keepAlive );
        }
        else if( method == "DELETE" )
        {
            sendResponse( sock, 405, "", "text/plain", keepAlive );
        }
        else if( method != "POST" )
        {
            sendResponse( sock, 405, "", "text/plain", keepAlive );
        }
        else
        {
            QJsonParseError perr;
            QJsonDocument doc = QJsonDocument::fromJson( body, &perr );
            if( perr.error != QJsonParseError::NoError )
            {
                QJsonObject e = makeError( QJsonValue(), -32700, "Parse error: "+perr.errorString() );
                sendResponse( sock, 400, QJsonDocument( e ).toJson( QJsonDocument::Compact ), "application/json", keepAlive );
            }
            else if( doc.isArray() )
            {
                QJsonArray out;
                for( const QJsonValue& v : doc.array() )
                {
                    QJsonValue r = v.isObject() ? handleMessage( v.toObject() )
                                 : QJsonValue( makeError( QJsonValue(), -32600, "Invalid Request" ) );
                    if( !r.isNull() ) out.append( r );
                }
                if( out.isEmpty() ) sendResponse( sock, 202, "", "text/plain", keepAlive );
                else sendResponse( sock, 200, QJsonDocument( out ).toJson( QJsonDocument::Compact ), "application/json", keepAlive );
            }
            else if( doc.isObject() )
            {
                QJsonValue r = handleMessage( doc.object() );
                if( r.isNull() ) sendResponse( sock, 202, "", "text/plain", keepAlive );
                else sendResponse( sock, 200, QJsonDocument( r.toObject() ).toJson( QJsonDocument::Compact ), "application/json", keepAlive );
            }
            else
            {
                QJsonObject e = makeError( QJsonValue(), -32600, "Invalid Request" );
                sendResponse( sock, 400, QJsonDocument( e ).toJson( QJsonDocument::Compact ), "application/json", keepAlive );
            }
        }
        if( !keepAlive || !m_buffers.contains( sock ) ) return;
    }
}

void McpServer::sendResponse( QTcpSocket* sock, int status, const QByteArray& body,
                              const QByteArray& contentType, bool keepAlive )
{
    QByteArray reason;
    switch( status )
    {
        case 200: reason = "OK"; break;
        case 202: reason = "Accepted"; break;
        case 204: reason = "No Content"; break;
        case 400: reason = "Bad Request"; break;
        case 403: reason = "Forbidden"; break;
        case 404: reason = "Not Found"; break;
        case 405: reason = "Method Not Allowed"; break;
        case 413: reason = "Payload Too Large"; break;
        case 431: reason = "Request Header Fields Too Large"; break;
        default:  reason = "Error";
    }
    QByteArray resp = "HTTP/1.1 " + QByteArray::number( status ) + " " + reason + "\r\n";
    if( !body.isEmpty() ) resp += "Content-Type: " + contentType + "\r\n";
    resp += "Content-Length: " + QByteArray::number( body.size() ) + "\r\n";
    if( status == 405 ) resp += "Allow: POST\r\n";
    resp += keepAlive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    resp += "\r\n";
    resp += body;
    sock->write( resp );
    sock->flush();
    if( !keepAlive )
    {
        m_buffers.remove( sock );
        sock->disconnectFromHost();
    }
}

QJsonObject McpServer::makeError( const QJsonValue& id, int code, const QString& msg )
{
    QJsonObject err;
    err["code"]    = code;
    err["message"] = msg;
    QJsonObject r;
    r["jsonrpc"] = "2.0";
    r["id"]      = id.isUndefined() ? QJsonValue() : id;
    r["error"]   = err;
    return r;
}

QJsonValue McpServer::handleMessage( const QJsonObject& msg )
{
    QString method = msg.value("method").toString();
    bool isRequest = msg.contains("id");
    QJsonValue id  = msg.value("id");

    if( method.isEmpty() )
    {
        if( msg.contains("result") || msg.contains("error") ) return QJsonValue(); // Client response: ignore
        return makeError( id, -32600, "Invalid Request" );
    }
    if( !isRequest ) return QJsonValue();             // Notifications (initialized, cancelled...)

    QJsonObject params = msg.value("params").toObject();
    QJsonObject result;

    if( method == "initialize" )
    {
        QJsonObject caps;
        caps["tools"] = QJsonObject();
        QJsonObject info;
        info["name"]    = "simulide";
        info["title"]   = "SimulIDE";
        info["version"] = QString( APP_VERSION );
        result["protocolVersion"] = kProtocolVersion;
        result["capabilities"]    = caps;
        result["serverInfo"]      = info;
        result["instructions"]    = "Drive the SimulIDE electronic circuit simulator. "
            "Use list_component_types to find component types, add_component to place them, "
            "get_circuit to inspect ids and pin names, connect_pins to wire them, then start_simulation.";
    }
    else if( method == "ping" ) { }
    else if( method == "tools/list" )
    {
        result["tools"] = m_tools->toolList();
    }
    else if( method == "tools/call" )
    {
        QString name = params.value("name").toString();
        if( !m_tools->hasTool( name ) ) return makeError( id, -32602, "Unknown tool: "+name );
        result = m_tools->call( name, params.value("arguments").toObject() );
    }
    else return makeError( id, -32601, "Method not found: "+method );

    QJsonObject r;
    r["jsonrpc"] = "2.0";
    r["id"]      = id;
    r["result"]  = result;
    return r;
}

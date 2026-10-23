/***************************************************************************
 *   MCP tool implementations for SimulIDE                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef MCPTOOLS_H
#define MCPTOOLS_H

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

class Component;
class Pin;

class McpTools : public QObject
{
    Q_OBJECT
  public:
    explicit McpTools(QObject *parent = 0l);

    QJsonArray toolList() const
    {
        return m_list;
    }
    bool hasTool(const QString &name) const
    {
        return m_handlers.contains(name);
    }
    QJsonObject call(const QString &name,
                     const QJsonObject &args); // MCP CallToolResult

  private:
    typedef QJsonObject (McpTools::*Handler)(const QJsonObject &);
    void reg(const QString &name, const QString &desc,
             const QJsonObject &schema, Handler h);

    QJsonObject listComponentTypes(const QJsonObject &a);
    QJsonObject getCircuit(const QJsonObject &a);
    QJsonObject addComponent(const QJsonObject &a);
    QJsonObject removeComponent(const QJsonObject &a);
    QJsonObject setProperty(const QJsonObject &a);
    QJsonObject connectPins(const QJsonObject &a);
    QJsonObject moveComponent(const QJsonObject &a);
    QJsonObject clearCircuit(const QJsonObject &a);
    QJsonObject loadCircuit(const QJsonObject &a);
    QJsonObject saveCircuit(const QJsonObject &a);
    QJsonObject startSimulation(const QJsonObject &a);
    QJsonObject stopSimulation(const QJsonObject &a);
    QJsonObject getSimulationState(const QJsonObject &a);
    QJsonObject getComponentState(const QJsonObject &a);
    QJsonObject loadFirmware(const QJsonObject &a);
    QJsonObject setCode(const QJsonObject &a);
    QJsonObject compileCode(const QJsonObject &a);
    QJsonObject screenshotCircuit(const QJsonObject &a);

    // helpers
    static QJsonObject ok(const QJsonValue &data);
    static QJsonObject fail(const QString &msg);
    static Component *findComp(const QString &id);
    static QList<Pin *> compPins(Component *comp);
    static QString pinName(Pin *pin);
    static Pin *findPin(Component *comp, const QString &name);
    static QJsonObject compToJson(Component *comp, bool withPins);
    static QJsonObject propsToJson(QObject *obj);
    static bool writeProp(QObject *obj, const QString &name,
                          const QJsonValue &v, QString *err);
    static void markChanged();

    QJsonArray m_list;
    QHash<QString, Handler> m_handlers;
};

#endif

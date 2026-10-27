/***************************************************************************
 *   MCP tool implementations for SimulIDE                                 *
 *   All handlers run on the GUI thread (called from McpServer slots).     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "mcptools.h"

#include "avrcompbase.h"
#include "circuit.h"
#include "circuitview.h"
#include "circuitwidget.h"
#include "codeeditor.h"
#include "component.h"
#include "componentselector.h"
#include "connector.h"
#include "editorwindow.h"
#include "ispuploader.h"
#include "itemlibrary.h"
#include "mainwindow.h"
#include "mcucomponent.h"
#include "outpaneltext.h"
#include "pin.h"
#include "simulator.h"
#include "spiceengine.h"

#include <QBuffer>
#include <QDir>
#include <QDomDocument>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QPainter>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>

// ---------------------------------------------------------------- schema
// helpers
static QJsonObject prop(const QString &type, const QString &desc)
{
    QJsonObject p;
    p["type"]        = type;
    p["description"] = desc;
    return p;
}
static QJsonObject schema(const QJsonObject &props    = QJsonObject(),
                          const QStringList &required = QStringList())
{
    QJsonObject s;
    s["type"]       = "object";
    s["properties"] = props;
    if (!required.isEmpty())
        s["required"] = QJsonArray::fromStringList(required);
    s["additionalProperties"] = false;
    return s;
}

McpTools::McpTools(QObject *parent) : QObject(parent)
{
    reg("list_component_types",
        "List all component types that can be added to the circuit, "
        "grouped with their category. Use the 'type' value with add_component.",
        schema(QJsonObject{
            { "category",
              prop(
                  "string",
                  "Optional category filter (case-insensitive substring)") } }),
        &McpTools::listComponentTypes);

    reg("get_circuit",
        "Return the current circuit: components (id, type, position, rotation, "
        "editable properties, pins with connection info) and wires (connectors "
        "between pins).",
        schema(QJsonObject{
            { "include_properties",
              prop("boolean",
                   "Include component properties (default true)") } }),
        &McpTools::getCircuit);

    {
        QJsonObject p;
        p["type"] =
            prop("string", "Component type, as returned by "
                           "list_component_types (e.g. 'Resistor', 'Led')");
        p["x"] = prop("number", "Scene X coordinate (snapped to 8px grid)");
        p["y"] = prop("number", "Scene Y coordinate (snapped to 8px grid)");
        p["rotation"] = prop("number", "Rotation in degrees (0, 90, 180, 270)");
        QJsonObject pr = prop(
            "object",
            "Optional property values by name (see get_circuit for names)");
        pr["additionalProperties"] = true;
        p["properties"]            = pr;
        reg("add_component",
            "Add a component of the given type at (x, y). Returns the new "
            "component with its id and pins.",
            schema(p, { "type", "x", "y" }), &McpTools::addComponent);
    }
    reg("remove_component",
        "Remove a component (and any wires attached to it) or a wire, by id.",
        schema(QJsonObject{ { "id",
                              prop("string", "Component or connector id") } },
               { "id" }),
        &McpTools::removeComponent);
    {
        QJsonObject p;
        p["id"]   = prop("string", "Component id");
        p["name"] = prop(
            "string", "Property name (e.g. 'Resistance', 'Color', 'Voltage')");
        QJsonObject v;
        v["description"] =
            "New value: number, boolean or string (enum values by name)";
        p["value"] = v;
        reg("set_property", "Set a property of a component.",
            schema(p, { "id", "name", "value" }), &McpTools::setProperty);
    }
    {
        QJsonObject p;
        p["compA"] = prop("string", "First component id");
        p["pinA"]  = prop(
            "string", "Pin name on first component (see pins in get_circuit)");
        p["compB"] = prop("string", "Second component id");
        p["pinB"]  = prop("string", "Pin name on second component");
        reg("connect_pins",
            "Draw a wire between two pins. Each pin can hold only one wire; "
            "to join 3+ pins, add a 'Node' component and wire each pin to it.",
            schema(p, { "compA", "pinA", "compB", "pinB" }),
            &McpTools::connectPins);
    }
    {
        QJsonObject p;
        p["id"]       = prop("string", "Component id");
        p["x"]        = prop("number", "New scene X");
        p["y"]        = prop("number", "New scene Y");
        p["rotation"] = prop("number", "Optional new rotation in degrees");
        reg("move_component", "Move (and optionally rotate) a component.",
            schema(p, { "id", "x", "y" }), &McpTools::moveComponent);
    }
    reg("clear_circuit",
        "Stop the simulation and remove everything (new empty circuit). "
        "Unsaved changes are discarded.",
        schema(), &McpTools::clearCircuit);
    reg("load_circuit",
        "Load a .simu circuit file (replaces the current circuit, unsaved "
        "changes discarded).",
        schema(QJsonObject{ { "path", prop("string",
                                           "Absolute path to a .simu file") } },
               { "path" }),
        &McpTools::loadCircuit);
    reg("save_circuit", "Save the current circuit to a .simu file.",
        schema(
            QJsonObject{
                { "path",
                  prop("string",
                       "Absolute path; '.simu' appended if missing") } },
            { "path" }),
        &McpTools::saveCircuit);
    reg("start_simulation", "Power on the circuit (start the simulation).",
        schema(), &McpTools::startSimulation);
    reg("stop_simulation", "Power off the circuit (stop the simulation).",
        schema(), &McpTools::stopSimulation);
    reg("get_simulation_state",
        "Return whether the simulation is running/paused, the simulation time "
        "and rates.",
        schema(), &McpTools::getSimulationState);
    reg("get_component_state",
        "Return runtime state of a component: pin voltages, connection state "
        "and current property values.",
        schema(QJsonObject{ { "id", prop("string", "Component id") } },
               { "id" }),
        &McpTools::getComponentState);
    {
        QJsonObject p;
        p["mcu_id"] =
            prop("string", "Id of an MCU component (AVR/PIC/Arduino)");
        p["hex_path"] = prop("string", "Absolute path to a firmware file "
                                       "(.hex, or .elf/.cod as supported)");
        reg("load_firmware", "Load a firmware file into an MCU component.",
            schema(p, { "mcu_id", "hex_path" }), &McpTools::loadFirmware);
    }
    {
        QJsonObject p;
        p["programmer_id"] = prop("string", "Id of the MCU running a STK500v1 "
                                            "programmer firmware (ArduinoISP)");
        p["hex_path"]   = prop("string", "Absolute path of the .hex to write "
                                           "into the target");
        p["target"]     = prop("string", "Target device, e.g. attiny85 "
                                             "(default: the other AVR in the circuit)");
        p["timeout_ms"] = prop("number", "Real time limit (default 120000)");
        reg("isp_program",
            "Program the AVR wired to the ISP pins of a simulated ArduinoISP "
            "using the built-in STK500v1 uploader (like avrdude -c stk500v1). "
            "Starts the simulation if needed and waits until done; returns the "
            "uploader log.",
            schema(p, { "programmer_id", "hex_path" }), &McpTools::ispProgram);
    }
    {
        QJsonObject p;
        p["path"] = prop("string", "Absolute path of the source file to write "
                                   "(e.g. /tmp/blink.asm, sketch.ino, .gcb)");
        p["code"] = prop("string", "Full source code");
        reg("set_code",
            "Write source code to a file and open it in the SimulIDE code "
            "editor.",
            schema(p, { "path", "code" }), &McpTools::setCode);
    }
    {
        QJsonObject p;
        p["mcu_id"] = prop("string", "Optional: bind the editor file to this "
                                     "MCU (upload/debug target)");
        reg("compile_code",
            "Compile the file currently open in the code editor and return the "
            "compiler output.",
            schema(p), &McpTools::compileCode);
    }
    {
        QJsonObject p;
        p["mcu_id"] = prop("string", "Id of the MCU to make active");
        reg("select_mcu",
            "List the MCUs in the circuit and optionally make one the active "
            "target (code editor upload/debug, RAM table, Serial Monitor).",
            schema(p), &McpTools::selectMcu);
    }
    {
        QJsonObject p;
        p["mode"] =
            prop("string", "'circuit' (default): render all items with margin; "
                           "'view': grab the visible circuit view");
        p["max_size"] =
            prop("number", "Maximum width/height in pixels (default 1600)");
        reg("screenshot_circuit", "Return a PNG image of the circuit.",
            schema(p), &McpTools::screenshotCircuit);
    }
    {
        QJsonObject p;
        p["engine"] =
            prop("string", "'native' (default real-time engine, works with "
                           "MCUs) or 'ngspice' (accurate analog, purely "
                           "analog circuits). Omit to only query.");
        reg("set_analog_engine",
            "Select the analog engine used by the next start_simulation and "
            "report ngspice availability (docs/ANALOG_ENGINE.md).",
            schema(p), &McpTools::setAnalogEngine);
    }
    reg("get_spice_netlist",
        "Return the SPICE netlist of the current circuit as exported for "
        "ngspice (or the reason it cannot be exported).",
        schema(), &McpTools::getSpiceNetlist);
}

void McpTools::reg(const QString &name, const QString &desc,
                   const QJsonObject &sch, Handler h)
{
    QJsonObject t;
    t["name"]        = name;
    t["description"] = desc;
    t["inputSchema"] = sch;
    m_list.append(t);
    m_handlers.insert(name, h);
}

QJsonObject McpTools::call(const QString &name, const QJsonObject &args)
{
    if (!Circuit::self())
        return fail("Circuit not available");
    Handler h = m_handlers.value(name);
    return (this->*h)(args);
}

// ---------------------------------------------------------------- helpers
QJsonObject McpTools::ok(const QJsonValue &data)
{
    QJsonObject c;
    c["type"] = "text";
    if (data.isString())
        c["text"] = data.toString();
    else if (data.isObject())
        c["text"] = QString::fromUtf8(
            QJsonDocument(data.toObject()).toJson(QJsonDocument::Indented));
    else if (data.isArray())
        c["text"] = QString::fromUtf8(
            QJsonDocument(data.toArray()).toJson(QJsonDocument::Indented));
    else
        c["text"] = "ok";
    QJsonObject r;
    r["content"] = QJsonArray{ c };
    if (data.isObject())
        r["structuredContent"] = data.toObject();
    r["isError"] = false;
    return r;
}

QJsonObject McpTools::fail(const QString &msg)
{
    QJsonObject c;
    c["type"] = "text";
    c["text"] = msg;
    QJsonObject r;
    r["content"] = QJsonArray{ c };
    r["isError"] = true;
    return r;
}

void McpTools::markChanged()
{
    Circuit::self()->setChanged();
    Circuit::self()->update();
}

Component *McpTools::findComp(const QString &id)
{
    for (Component *c : *Circuit::self()->compList())
        if (c->itemID() == id || c->objectName() == id)
            return c;
    for (Component *c : *Circuit::self()->conList())
        if (c->itemID() == id || c->objectName() == id)
            return c;
    // Fall back to the visible label
    for (Component *c : *Circuit::self()->compList())
        if (c->idLabel() == id)
            return c;
    return 0l;
}

QList<Pin *> McpTools::compPins(Component *comp)
{
    QList<Pin *> pins;
    QList<QGraphicsItem *> stack = comp->childItems();
    while (!stack.isEmpty()) {
        QGraphicsItem *it = stack.takeFirst();
        if (it->type() == Pin::Type) {
            Pin *p = static_cast<Pin *>(it);
            if (p->component() == comp && !pins.contains(p))
                pins.append(p);
        }
        stack.append(it->childItems());
    }
    return pins;
}

QString McpTools::pinName(Pin *pin)
{
    QString n = pin->objectName();
    if (n.isEmpty())
        n = pin->pinId();
    Component *c = pin->component();
    if (c) {
        QString pre = c->itemID() + "-";
        if (n.startsWith(pre))
            n = n.mid(pre.size());
    }
    return n;
}

Pin *McpTools::findPin(Component *comp, const QString &name)
{
    QList<Pin *> pins = compPins(comp);
    for (Pin *p : pins)
        if (pinName(p) == name || p->objectName() == name)
            return p;
    for (Pin *p : pins)
        if (pinName(p).compare(name, Qt::CaseInsensitive) == 0)
            return p;
    for (Pin *p : pins)
        if (!p->getLabelText().isEmpty() && p->getLabelText().trimmed() == name)
            return p;
    bool isNum = false;
    int idx    = name.toInt(&isNum); // Allow index
    if (isNum && idx >= 0 && idx < pins.size())
        return pins[idx];
    return 0l;
}

QJsonObject McpTools::propsToJson(QObject *obj)
{
    QJsonObject props;
    const QMetaObject *mo = obj->metaObject();
    for (int i = 0; i < mo->propertyCount(); ++i) {
        QMetaProperty mp = mo->property(i);
        if (!mp.isUser() || !mp.isDesignable() || !mp.isWritable())
            continue;
        QString n = mp.name();
        if (n == "id" || n == "Show_id" || n == "itemtype")
            continue;
        QVariant v = mp.read(obj);
        if (mp.isEnumType()) {
            QMetaEnum me    = mp.enumerator();
            const char *key = me.valueToKey(v.toInt());
            props[n]        = key ? QString(key) : QString::number(v.toInt());
        } else if (v.type() == QVariant::Bool)
            props[n] = v.toBool();
        else if (v.canConvert<double>() &&
                 (v.type() == QVariant::Int || v.type() == QVariant::Double ||
                  v.type() == QVariant::UInt ||
                  v.type() == QVariant::LongLong ||
                  v.type() == QVariant::ULongLong ||
                  (int)v.type() == QMetaType::Float))
            props[n] = v.toDouble();
        else if (v.canConvert<QString>())
            props[n] = v.toString();
    }
    return props;
}

bool McpTools::writeProp(QObject *obj, const QString &name,
                         const QJsonValue &val, QString *err)
{
    const QMetaObject *mo = obj->metaObject();
    int idx               = mo->indexOfProperty(name.toUtf8().constData());
    if (idx < 0) // case-insensitive lookup
    {
        for (int i = 0; i < mo->propertyCount(); ++i)
            if (QString(mo->property(i).name())
                    .compare(name, Qt::CaseInsensitive) == 0) {
                idx = i;
                break;
            }
    }
    if (idx < 0) {
        *err = "Unknown property '" + name + "'";
        return false;
    }
    QMetaProperty mp = mo->property(idx);
    if (!mp.isWritable()) {
        *err = "Property '" + name + "' is read-only";
        return false;
    }

    QVariant v;
    if (mp.isEnumType()) {
        QMetaEnum me = mp.enumerator();
        int iv       = -1;
        if (val.isDouble())
            iv = val.toInt();
        else {
            bool okK = false;
            for (int k = 0; k < me.keyCount();
                 ++k) // case-insensitive key match
                if (QString(me.key(k)).compare(val.toString(),
                                               Qt::CaseInsensitive) == 0) {
                    iv  = me.value(k);
                    okK = true;
                    break;
                }
            if (!okK) {
                QStringList keys;
                for (int k = 0; k < me.keyCount(); ++k)
                    keys << me.key(k);
                *err = "Invalid value for enum '" + name +
                       "'. Allowed: " + keys.join(", ");
                return false;
            }
        }
        v        = QVariant(iv);
        bool okW = mp.write(obj, v);
        if (!okW) {
            *err = "Could not write enum property '" + name + "'";
            return false;
        }
        return true;
    }
    int t = mp.userType();
    if (val.isBool())
        v = val.toBool();
    else if (val.isDouble())
        v = val.toDouble();
    else if (val.isString())
        v = val.toString();
    else if (val.isArray()) {
        QStringList sl;
        for (const QJsonValue &e : val.toArray())
            sl << e.toVariant().toString();
        v = sl;
    } else {
        *err = "Unsupported value type";
        return false;
    }

    if (t == QMetaType::Bool && val.isString()) {
        QString s = val.toString().toLower();
        v         = (s == "true" || s == "1" || s == "yes" || s == "on");
    }
    if (!v.convert(t)) {
        *err = "Cannot convert value to type " + QString(mp.typeName());
        return false;
    }
    if (!mp.write(obj, v)) {
        *err = "Could not write property '" + name + "'";
        return false;
    }
    return true;
}

QJsonObject McpTools::compToJson(Component *comp, bool withProps)
{
    QJsonObject o;
    o["id"]   = comp->itemID();
    o["type"] = comp->itemType();
    if (comp->idLabel() != comp->itemID())
        o["label"] = comp->idLabel();
    o["x"]        = comp->pos().x();
    o["y"]        = comp->pos().y();
    o["rotation"] = comp->rotation();
    if (withProps)
        o["properties"] = propsToJson(comp);

    QJsonArray pins;
    for (Pin *p : compPins(comp)) {
        QJsonObject po;
        po["name"] = pinName(p);
        if (!p->getLabelText().trimmed().isEmpty())
            po["label"] = p->getLabelText().trimmed();
        QPointF sp      = p->scenePos();
        po["x"]         = sp.x();
        po["y"]         = sp.y();
        Connector *con  = p->connector();
        po["connected"] = (con != 0l);
        if (con) {
            po["wire"] = con->itemID();
            Pin *other =
                (con->startPin() == p) ? con->endPin() : con->startPin();
            if (other && other->component())
                po["to"] = other->component()->itemID() + ":" + pinName(other);
        }
        if (p->unused())
            po["unused"] = true;
        pins.append(po);
    }
    o["pins"] = pins;
    return o;
}

// ---------------------------------------------------------------- tools
QJsonObject McpTools::listComponentTypes(const QJsonObject &a)
{
    QString filter = a.value("category").toString();
    QJsonArray arr;
    for (LibraryItem *li : ItemLibrary::self()->items()) {
        if (li->type().isEmpty())
            continue;
        if (!filter.isEmpty() &&
            !li->category().contains(filter, Qt::CaseInsensitive))
            continue;
        QJsonObject o;
        o["type"]     = li->type();
        o["name"]     = li->name();
        o["category"] = li->category();
        arr.append(o);
    }
    QJsonObject r;
    r["component_types"] = arr;
    r["count"]           = arr.size();
    return ok(r);
}

QJsonObject McpTools::getCircuit(const QJsonObject &a)
{
    bool withProps = a.contains("include_properties")
                         ? a.value("include_properties").toBool()
                         : true;
    Circuit *circ  = Circuit::self();

    QJsonArray comps;
    for (Component *c : *circ->compList())
        comps.append(compToJson(c, withProps));

    QJsonArray wires;
    for (Component *cc : *circ->conList()) {
        Connector *con = static_cast<Connector *>(cc);
        QJsonObject w;
        w["id"] = con->itemID();
        Pin *sp = con->startPin();
        Pin *ep = con->endPin();
        if (sp && sp->component()) {
            w["from_component"] = sp->component()->itemID();
            w["from_pin"]       = pinName(sp);
        }
        if (ep && ep->component()) {
            w["to_component"] = ep->component()->itemID();
            w["to_pin"]       = pinName(ep);
        }
        if (Simulator::self()->isRunning())
            w["voltage"] = con->getVolt();
        wires.append(w);
    }
    QJsonObject r;
    r["file"]               = circ->getFileName();
    r["components"]         = comps;
    r["wires"]              = wires;
    r["simulation_running"] = Simulator::self()->isRunning();
    return ok(r);
}

QJsonObject McpTools::addComponent(const QJsonObject &a)
{
    QString type = a.value("type").toString();
    if (type.isEmpty())
        return fail("Missing 'type'");
    if (!a.value("x").isDouble() || !a.value("y").isDouble())
        return fail("'x' and 'y' must be numbers");

    LibraryItem *li = 0l;
    for (LibraryItem *it : ItemLibrary::self()->items())
        if (it->type() == type) {
            li = it;
            break;
        }
    if (!li)
        for (LibraryItem *it : ItemLibrary::self()->items())
            if (it->type().compare(type, Qt::CaseInsensitive) == 0 ||
                it->name().compare(type, Qt::CaseInsensitive) == 0) {
                li = it;
                break;
            }
    // Sub-models defined in xml sets (e.g. "Arduino Uno", "atmega328",
    // "attiny85"): resolve the parent library type, id must keep the name.
    QString model;
    if (!li) {
        QString parentType = xmlParentType(type);
        for (LibraryItem *it : ItemLibrary::self()->items())
            if (!parentType.isEmpty() && it->type() == parentType) {
                li    = it;
                model = type;
                break;
            }
    }
    if (!li)
        return fail("Unknown component type '" + type +
                    "'. Use list_component_types.");

    Circuit *circ = Circuit::self();
    if (circ->is_constarted())
        circ->deleteNewConnector();

    QString name = li->type(); // Ids are "<Type>-<n>", safe characters only
    name.remove(QRegularExpression("[^A-Za-z0-9_]"));
    if (name.isEmpty())
        name = "Comp";
    if (!model.isEmpty())
        name = model;
    QString id = name + "-" + circ->newSceneId();

    circ->saveState();
    Component *comp = circ->createItem(li->type(), id);
    if (!comp)
        return fail("Could not create component of type '" + li->type() +
                    "' (only one instance may be allowed)");

    if (li->type() == "Subcircuit") {
    }
    int gx = qRound(a.value("x").toDouble() / 8) * 8;
    int gy = qRound(a.value("y").toDouble() / 8) * 8;
    comp->setPos(QPointF(gx, gy));
    circ->addItem(comp);
    if (a.value("rotation").isDouble())
        comp->setRotation(a.value("rotation").toDouble());

    QStringList errors;
    QJsonObject props = a.value("properties").toObject();
    for (auto it = props.begin(); it != props.end(); ++it) {
        QString err;
        if (!writeProp(comp, it.key(), it.value(), &err))
            errors << err;
    }
    comp->moveTo(comp->pos());
    markChanged();

    QJsonObject r = compToJson(comp, true);
    if (!errors.isEmpty())
        r["property_errors"] = QJsonArray::fromStringList(errors);
    return ok(r);
}

QJsonObject McpTools::removeComponent(const QJsonObject &a)
{
    QString id      = a.value("id").toString();
    Component *comp = findComp(id);
    if (!comp)
        return fail("No component with id '" + id + "'");
    Circuit *circ = Circuit::self();
    if (circ->is_constarted())
        circ->deleteNewConnector();

    bool pauseSim = Simulator::self()->isRunning();
    if (pauseSim)
        Simulator::self()->pauseSim();
    circ->saveState();

    if (comp->itemType() == "Connector")
        comp->remove();
    else
        circ->removeComp(comp);

    if (pauseSim)
        Simulator::self()->resumeSim();
    markChanged();
    if (findComp(id))
        return fail("Component '" + id +
                    "' could not be removed (try stopping the simulation)");
    return ok(QString("Removed " + id));
}

QJsonObject McpTools::setProperty(const QJsonObject &a)
{
    QString id      = a.value("id").toString();
    Component *comp = findComp(id);
    if (!comp)
        return fail("No component with id '" + id + "'");
    if (!a.contains("value"))
        return fail("Missing 'value'");
    Circuit::self()->saveState();
    QString err;
    if (!writeProp(comp, a.value("name").toString(), a.value("value"), &err))
        return fail(err);
    comp->update();
    markChanged();
    QJsonObject r;
    r["id"]         = comp->itemID();
    r["properties"] = propsToJson(comp);
    return ok(r);
}

QJsonObject McpTools::connectPins(const QJsonObject &a)
{
    Component *ca = findComp(a.value("compA").toString());
    Component *cb = findComp(a.value("compB").toString());
    if (!ca)
        return fail("No component with id '" + a.value("compA").toString() +
                    "'");
    if (!cb)
        return fail("No component with id '" + a.value("compB").toString() +
                    "'");

    Pin *pa         = findPin(ca, a.value("pinA").toString());
    Pin *pb         = findPin(cb, a.value("pinB").toString());
    auto pinListStr = [](Component *c) {
        QStringList l;
        for (Pin *p : compPins(c))
            l << pinName(p);
        return l.join(", ");
    };
    if (!pa)
        return fail("No pin '" + a.value("pinA").toString() + "' on " +
                    ca->itemID() + ". Pins: " + pinListStr(ca));
    if (!pb)
        return fail("No pin '" + a.value("pinB").toString() + "' on " +
                    cb->itemID() + ". Pins: " + pinListStr(cb));
    if (pa == pb)
        return fail("Cannot connect a pin to itself");
    if (pa->unused() || pb->unused())
        return fail("Pin is marked unused");
    if (pa->connector())
        return fail(ca->itemID() + ":" + pinName(pa) +
                    " already has a wire. Each pin holds one wire; add a "
                    "'Node' component to branch.");
    if (pb->connector())
        return fail(cb->itemID() + ":" + pinName(pb) +
                    " already has a wire. Each pin holds one wire; add a "
                    "'Node' component to branch.");
    if (pa->isBus() != pb->isBus())
        return fail("Cannot connect a bus pin to a non-bus pin");

    Circuit *circ = Circuit::self();
    if (circ->is_constarted())
        circ->deleteNewConnector();

    bool pauseSim = Simulator::self()->isRunning();
    if (pauseSim)
        Simulator::self()->pauseSim();

    circ->newconnector(pa); // Same path as user clicks
    Connector *con = circ->getNewConnector();
    con->updateConRoute(0l, pb->scenePos());
    circ->closeconnector(pb);
    con->updateConRoute(pb, pb->scenePos()); // Final route ends at the pin

    if (pauseSim)
        Simulator::self()->resumeSim();
    markChanged();

    QJsonObject r;
    r["wire"] = con->itemID();
    r["from"] = ca->itemID() + ":" + pinName(pa);
    r["to"]   = cb->itemID() + ":" + pinName(pb);
    return ok(r);
}

QJsonObject McpTools::moveComponent(const QJsonObject &a)
{
    QString id      = a.value("id").toString();
    Component *comp = findComp(id);
    if (!comp)
        return fail("No component with id '" + id + "'");
    if (comp->itemType() == "Connector")
        return fail("Wires cannot be moved directly");
    if (!a.value("x").isDouble() || !a.value("y").isDouble())
        return fail("'x' and 'y' must be numbers");
    Circuit::self()->saveState();
    int gx = qRound(a.value("x").toDouble() / 8) * 8;
    int gy = qRound(a.value("y").toDouble() / 8) * 8;
    if (a.value("rotation").isDouble())
        comp->setRotation(a.value("rotation").toDouble());
    comp->moveTo(QPointF(gx, gy));
    for (Pin *p : compPins(comp))
        p->isMoved();
    markChanged();
    return ok(compToJson(comp, false));
}

QJsonObject McpTools::clearCircuit(const QJsonObject &)
{
    MainWindow::self()->setTitle(
        "New Circuit"); // drop '*' => no "save changes?" dialog
    CircuitWidget::self()->newCircuit();
    return ok(QString("Circuit cleared"));
}

QJsonObject McpTools::loadCircuit(const QJsonObject &a)
{
    QString path = a.value("path").toString();
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
        return fail("File not found: " + path);
    if (!path.endsWith(".simu"))
        return fail("Circuit files must have the .simu extension");
    MainWindow::self()->setTitle("New Circuit");
    CircuitWidget::self()->loadCirc(fi.absoluteFilePath());
    QJsonObject r;
    r["file"]       = Circuit::self()->getFileName();
    r["components"] = Circuit::self()->compList()->size();
    r["wires"]      = Circuit::self()->conList()->size();
    return ok(r);
}

QJsonObject McpTools::saveCircuit(const QJsonObject &a)
{
    QString path = a.value("path").toString();
    if (path.isEmpty())
        return fail("Missing 'path'");
    if (!path.endsWith(".simu"))
        path.append(".simu");
    QFileInfo fi(path);
    if (!fi.absoluteDir().exists())
        return fail("Directory does not exist: " + fi.absolutePath());
    QString p = fi.absoluteFilePath();
    if (!Circuit::self()->saveCircuit(p))
        return fail("Could not save circuit to " + p);
    MainWindow::self()->setTitle(p.split("/").last());
    return ok(QString("Saved to " + p));
}

QJsonObject McpTools::startSimulation(const QJsonObject &)
{
    if (Simulator::self()->isRunning())
        return ok(QString("Simulation already running"));
    CircuitWidget::self()->powerCircOn();
    if (!Simulator::self()->isRunning())
        return fail("Simulation failed to start (check the circuit)");
    return getSimulationState(QJsonObject());
}

QJsonObject McpTools::stopSimulation(const QJsonObject &)
{
    CircuitWidget::self()->powerCircOff();
    return getSimulationState(QJsonObject());
}

QJsonObject McpTools::getSimulationState(const QJsonObject &)
{
    Simulator *sim = Simulator::self();
    QJsonObject r;
    r["running"]         = sim->isRunning();
    r["paused"]          = sim->isPaused();
    r["step"]            = (double)sim->step();
    r["circuit_time_ps"] = (double)sim->circTime();
    r["circuit_rate"]    = sim->circuitRate();
    r["simu_rate"]       = sim->simuRate();
    return ok(r);
}

QJsonObject McpTools::getComponentState(const QJsonObject &a)
{
    QString id      = a.value("id").toString();
    Component *comp = findComp(id);
    if (!comp)
        return fail("No component with id '" + id + "'");
    bool running = Simulator::self()->isRunning();

    QJsonObject r   = compToJson(comp, true);
    QJsonArray pins = r.value("pins").toArray();
    QList<Pin *> pl = compPins(comp);
    QJsonArray outPins;
    for (int i = 0; i < pins.size() && i < pl.size(); ++i) {
        QJsonObject po = pins[i].toObject();
        Pin *p         = pl[i];
        if (p->isConnected() && p->getEnode())
            po["voltage"] = p->getVolt();
        outPins.append(po);
    }
    r["pins"]               = outPins;
    r["simulation_running"] = running;

    // Two-pin components: voltage across
    if (pl.size() == 2 && pl[0]->getEnode() && pl[1]->getEnode())
        r["voltage_across"] = pl[0]->getVolt() - pl[1]->getVolt();

    if (comp->itemType() == "Connector")
        r["voltage"] = static_cast<Connector *>(comp)->getVolt();

    McuComponent *mcu = dynamic_cast<McuComponent *>(comp);
    if (mcu) {
        r["firmware"] = mcu->program();
        r["device"]   = mcu->device();
        r["active"]   = (mcu == McuComponent::self());
    }
    AvrCompBase *avr = dynamic_cast<AvrCompBase *>(comp);
    if (avr && avr->avrProcessor()->getCpu()) {
        AvrProcessor *ap = avr->avrProcessor();
        avr_t *cpu       = ap->getCpu();
        QJsonObject isp;
        isp["blank"]              = ap->isBlank();
        isp["in_reset"]           = ap->isp()->inReset();
        isp["prog_enabled"]       = ap->isp()->progEnabled();
        isp["reset_pin_function"] = avr->resetFunction();
        QString head;
        for (int i = 0; i < 16 && i <= (int)cpu->flashend; ++i)
            head += QString("%1 ").arg(cpu->flash[i], 2, 16, QChar('0'));
        isp["flash_head"] = head.trimmed();
        isp["fuses"]      = QString("L=%1 H=%2 E=%3 lock=%4")
                           .arg(cpu->fuse[0], 2, 16, QChar('0'))
                           .arg(cpu->fuse[1], 2, 16, QChar('0'))
                           .arg(cpu->fuse[2], 2, 16, QChar('0'))
                           .arg(cpu->lockbits, 2, 16, QChar('0'));
        r["avr"] = isp;
    }

    if (!running)
        r["note"] = "Simulation is not running: voltages are not meaningful.";
    return ok(r);
}

QJsonObject McpTools::loadFirmware(const QJsonObject &a)
{
    QString id      = a.value("mcu_id").toString();
    Component *comp = findComp(id);
    if (!comp)
        return fail("No component with id '" + id + "'");
    McuComponent *mcu = dynamic_cast<McuComponent *>(comp);
    if (!mcu)
        return fail("Component '" + id + "' is not an MCU");
    QString path = a.value("hex_path").toString();
    QFileInfo fi(path);
    if (!fi.exists())
        return fail("File not found: " + path);
    mcu->load(fi.absoluteFilePath());
    QJsonObject r;
    r["mcu_id"]   = comp->itemID();
    r["firmware"] = mcu->program();
    if (mcu->program().isEmpty())
        return fail("Firmware could not be loaded into " + id);
    return ok(r);
}

QJsonObject McpTools::ispProgram(const QJsonObject &a)
{
    QString id        = a.value("programmer_id").toString();
    Component *comp   = findComp(id);
    McuComponent *mcu = dynamic_cast<McuComponent *>(comp);
    if (!mcu)
        return fail("No MCU with id '" + id + "'");
    QFileInfo fi(a.value("hex_path").toString());
    if (!fi.exists())
        return fail("File not found: " + fi.filePath());
    QString target = a.value("target").toString();
    if (target.isEmpty())
        target = IspUploader::guessTarget(mcu);
    int timeout = a.value("timeout_ms").toInt(120000);

    if (!Simulator::self()->isRunning())
        CircuitWidget::self()->powerCircOn();

    IspUploader up(mcu);
    if (!up.start(fi.absoluteFilePath(), target))
        return fail(up.error());

    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    connect(&up, &IspUploader::finished, &loop, &QEventLoop::quit);
    connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    guard.start(timeout);
    QElapsedTimer clock;
    clock.start();
    if (up.running())
        loop.exec();

    QJsonObject r;
    r["target"]  = target;
    r["log"]     = up.log();
    r["real_ms"] = (double)clock.elapsed();
    if (up.running())
        return fail("Timeout. Log so far:\n" + up.log());
    if (!up.ok())
        return fail(up.error() + "\n" + up.log());
    r["ok"] = true;
    return ok(r);
}

QJsonObject McpTools::setCode(const QJsonObject &a)
{
    EditorWindow *ed = EditorWindow::self();
    if (!ed)
        return fail("Code editor not available");
    QString path = a.value("path").toString();
    if (path.isEmpty())
        return fail("Missing 'path'");
    QFileInfo fi(path);
    if (!fi.absoluteDir().exists())
        return fail("Directory does not exist: " + fi.absolutePath());
    QFile f(fi.absoluteFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return fail("Cannot write " + path + ": " + f.errorString());
    f.write(a.value("code").toString().toUtf8());
    f.close();
    ed->loadFile(fi.absoluteFilePath());
    return ok(QString("Wrote and opened " + fi.absoluteFilePath()));
}

QString McpTools::xmlParentType(const QString &model)
{
    QString file = ComponentSelector::self()->getXmlFile(model);
    QFile f(file);
    if (file.isEmpty() || !f.open(QFile::ReadOnly | QFile::Text))
        return QString();
    QDomDocument doc;
    if (!doc.setContent(&f))
        return QString();
    QDomNode r = doc.documentElement().firstChild();
    for (; !r.isNull(); r = r.nextSibling()) {
        QDomElement cat = r.toElement();
        for (QDomNode n = cat.firstChild(); !n.isNull(); n = n.nextSibling())
            if (n.toElement().attribute("name") == model)
                return cat.attribute("type");
    }
    return QString();
}

QJsonObject McpTools::selectMcu(const QJsonObject &a)
{
    QString id = a.value("mcu_id").toString();
    if (!id.isEmpty()) {
        McuComponent *mcu = McuComponent::findMcu(id);
        if (!mcu)
            return fail("No MCU with id '" + id + "'");
        McuComponent::setSelf(mcu);
    }
    QJsonArray list;
    for (McuComponent *mcu : McuComponent::mcuList()) {
        QJsonObject o;
        o["id"]       = mcu->itemID();
        o["device"]   = mcu->device();
        o["firmware"] = mcu->program();
        o["active"]   = (mcu == McuComponent::self());
        list.append(o);
    }
    QJsonObject r;
    r["mcus"]   = list;
    r["active"] = McuComponent::self() ? McuComponent::self()->itemID() : "";
    return ok(r);
}

QJsonObject McpTools::compileCode(const QJsonObject &a)
{
    EditorWindow *ed = EditorWindow::self();
    if (!ed)
        return fail("Code editor not available");
    CodeEditor *ce = ed->getCodeEditor();
    if (!ce)
        return fail("No file open in the code editor (use set_code)");
    QString mcuId = a.value("mcu_id").toString();
    if (!mcuId.isEmpty()) {
        McuComponent *mcu = McuComponent::findMcu(mcuId);
        if (!mcu)
            return fail("No MCU with id '" + mcuId + "'");
        ce->setTargetMcuId(mcuId);
        McuComponent::setSelf(mcu);
    }
    OutPanelText *out = ed->findChild<OutPanelText *>();
    int before        = out ? out->toPlainText().size() : 0;
    ce->compile();
    QString log = out ? out->toPlainText().mid(before) : QString();
    QJsonObject r;
    r["output"] = log;
    bool error =
        log.contains("ERROR", Qt::CaseInsensitive) || log.contains("error:");
    if (error) {
        QJsonObject f = fail("Compilation reported errors:\n" + log);
        return f;
    }
    return ok(r);
}

QJsonObject McpTools::screenshotCircuit(const QJsonObject &a)
{
    QString mode = a.value("mode").toString("circuit");
    int maxSize =
        a.value("max_size").isDouble() ? a.value("max_size").toInt() : 1600;
    if (maxSize < 64)
        maxSize = 64;

    QImage img;
    Circuit *circ = Circuit::self();
    QRectF rect   = circ->itemsBoundingRect();

    if (mode == "view" || rect.isEmpty()) {
        if (!CircuitView::self())
            return fail("Circuit view not available");
        img = CircuitView::self()->viewport()->grab().toImage();
    } else {
        rect.adjust(-24, -24, 24, 24);
        double scale = 2.0;
        if (rect.width() * scale > maxSize || rect.height() * scale > maxSize)
            scale = qMin(maxSize / rect.width(), maxSize / rect.height());
        QSize sz(qMax(1, (int)(rect.width() * scale)),
                 qMax(1, (int)(rect.height() * scale)));
        img = QImage(sz, QImage::Format_ARGB32);
        img.fill(QColor(240, 240, 210));
        QList<QGraphicsItem *> sel = circ->selectedItems();
        for (QGraphicsItem *it : sel)
            it->setSelected(false);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        circ->render(&p, QRectF(QPointF(0, 0), sz), rect);
        p.end();
        for (QGraphicsItem *it : sel)
            it->setSelected(true);
    }
    if (img.width() > maxSize || img.height() > maxSize)
        img = img.scaled(maxSize, maxSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);

    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");

    QJsonObject c;
    c["type"]     = "image";
    c["mimeType"] = "image/png";
    c["data"]     = QString::fromLatin1(png.toBase64());
    QJsonObject t;
    t["type"] = "text";
    t["text"] =
        QString("Circuit screenshot %1x%2").arg(img.width()).arg(img.height());
    QJsonObject r;
    r["content"] = QJsonArray{ c, t };
    r["isError"] = false;
    return r;
}

QJsonObject McpTools::setAnalogEngine(const QJsonObject &a)
{
    QString engine = a.value("engine").toString().toLower();
    if (!engine.isEmpty()) {
        if (engine != "native" && engine != "ngspice")
            return fail("engine must be 'native' or 'ngspice'");
        bool running = Simulator::self()->isRunning();
        if (running)
            CircuitWidget::self()->powerCircOff();
        SpiceEngine::setEnabled(engine == "ngspice");
        if (running)
            CircuitWidget::self()->powerCircOn();
    }
    QString err;
    bool avail = SpiceEngine::self()->available(&err);
    QJsonObject r;
    r["engine"]            = SpiceEngine::enabled() ? "ngspice" : "native";
    r["ngspice_available"] = avail;
    if (avail)
        r["ngspice_version"] = SpiceEngine::self()->version();
    else
        r["ngspice_error"] = err;
    r["ngspice_active"] = Simulator::self()->spiceMode();
    return ok(r);
}

QJsonObject McpTools::getSpiceNetlist(const QJsonObject &)
{
    QString err;
    QString text = SpiceEngine::self()->netlistText(&err);
    QJsonObject r;
    r["netlist"] = text;
    if (!err.isEmpty())
        r["error"] = err;
    r["ngspice_active"] = Simulator::self()->spiceMode();
    if (Simulator::self()->spiceMode())
        r["ngspice_log"] = SpiceEngine::self()->log();
    return ok(r);
}

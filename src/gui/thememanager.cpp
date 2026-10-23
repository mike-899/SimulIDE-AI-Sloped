/***************************************************************************
 *   Material Design 3 theme manager (light / dark)                        *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#include "thememanager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QSvgRenderer>

// ---- M3 tonal palettes (seed #1A73E8) ----------------------------------
// Roles follow the Material 3 colour system, including the surface
// container levels introduced in 2023.
static const QHash<QString, QString> &lightTokens()
{
    static const QHash<QString, QString> t = {
        { "primary", "#0B57D0" },
        { "onPrimary", "#FFFFFF" },
        { "primaryContainer", "#D3E3FD" },
        { "onPrimaryContainer", "#041E49" },
        { "secondaryContainer", "#D9E2FB" },
        { "onSecondaryContainer", "#131C2B" },
        { "surface", "#FAF9FD" },
        { "surfaceContainerLowest", "#FFFFFF" },
        { "surfaceContainerLow", "#F4F3F8" },
        { "surfaceContainer", "#EEEDF3" },
        { "surfaceContainerHigh", "#E8E7ED" },
        { "surfaceContainerHighest", "#E2E2E8" },
        { "onSurface", "#1A1B20" },
        { "onSurfaceVariant", "#44474F" },
        { "outline", "#74777F" },
        { "outlineVariant", "#C4C6D0" },
        { "inverseSurface", "#2F3036" },
        { "inverseOnSurface", "#F1F0F7" },
        { "success", "#1E8E3E" },
        { "danger", "#C5221F" },
        { "scrollHandle", "#C4C6D0" },
    };
    return t;
}

static const QHash<QString, QString> &darkTokens()
{
    static const QHash<QString, QString> t = {
        { "primary", "#A8C7FA" },
        { "onPrimary", "#062E6F" },
        { "primaryContainer", "#0842A0" },
        { "onPrimaryContainer", "#D3E3FD" },
        { "secondaryContainer", "#3D4759" },
        { "onSecondaryContainer", "#D9E2FB" },
        { "surface", "#121316" },
        { "surfaceContainerLowest", "#0D0E11" },
        { "surfaceContainerLow", "#1A1B1F" },
        { "surfaceContainer", "#1E1F23" },
        { "surfaceContainerHigh", "#292A2E" },
        { "surfaceContainerHighest", "#333539" },
        { "onSurface", "#E3E2E8" },
        { "onSurfaceVariant", "#C4C6D0" },
        { "outline", "#8E9099" },
        { "outlineVariant", "#44474F" },
        { "inverseSurface", "#E3E2E8" },
        { "inverseOnSurface", "#2F3036" },
        { "success", "#1E8E3E" },
        { "danger", "#C5221F" },
        { "scrollHandle", "#44474F" },
    };
    return t;
}

static QColor blend(const QColor &base, const QColor &over, qreal a)
{
    return QColor::fromRgbF(base.redF() * (1 - a) + over.redF() * a,
                            base.greenF() * (1 - a) + over.greenF() * a,
                            base.blueF() * (1 - a) + over.blueF() * a);
}

static QString rgba(const QColor &c, qreal a)
{
    return QString("rgba(%1, %2, %3, %4)")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue())
        .arg(int(a * 255));
}

ThemeManager *ThemeManager::self()
{
    static ThemeManager *inst = 0l;
    if (!inst)
        inst = new ThemeManager(qApp);
    return inst;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent)
{
    m_theme    = Light;
    m_settings = 0l;
    m_applied  = false;
}

QColor ThemeManager::token(const QString &name) const
{
    const QHash<QString, QString> &t = isDark() ? darkTokens() : lightTokens();
    return QColor(t.value(name, "#FF00FF"));
}

void ThemeManager::init(QSettings *settings)
{
    m_settings = settings;
    if (m_settings && m_settings->contains("theme"))
        m_theme =
            (m_settings->value("theme").toString() == "dark") ? Dark : Light;

    QStyle *fusion = QStyleFactory::create("Fusion");
    if (fusion)
        qApp->setStyle(fusion);

    // Bundled Inter (SIL OFL 1.1) so the UI looks the same everywhere
    const QStringList fonts = { ":/fonts/Inter-Regular.otf",
                                ":/fonts/Inter-Medium.otf",
                                ":/fonts/Inter-SemiBold.otf" };
    for (const QString &f : fonts)
        QFontDatabase::addApplicationFont(f);

    QFont font = qApp->font();
    font.setFamily("Inter");
    font.setHintingPreference(QFont::PreferNoHinting);
    qApp->setFont(font);

    apply();
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_applied && theme == m_theme)
        return;
    m_theme = theme;
    if (m_settings)
        m_settings->setValue("theme", themeName());
    apply();
}

// Writes ":/material/<name>.svg" tinted with colour to a cache folder and
// returns a QSS url() for it (QSS images cannot be recoloured).
static QString tintedSvgUrl(const QString &name, const QColor &color)
{
    static QString dir;
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (dir.isEmpty())
            dir = QDir::tempPath() + "/simulide";
        dir += "/m3icons";
        QDir().mkpath(dir);
    }
    QString file = QString("%1/%2_%3.svg").arg(dir, name, color.name().mid(1));
    if (!QFile::exists(file)) {
        QFile in(QString(":/material/%1.svg").arg(name));
        if (!in.open(QFile::ReadOnly))
            return "none";
        QString svg = QString::fromUtf8(in.readAll());
        svg.replace("<svg ", QString("<svg fill=\"%1\" ").arg(color.name()));
        QFile out(file);
        if (out.open(QFile::WriteOnly))
            out.write(svg.toUtf8());
    }
    return QString("url(%1)").arg(file);
}

void ThemeManager::apply()
{
    bool dark = isDark();

    // Token table: base roles + derived state-layer colours
    QHash<QString, QString> tk = dark ? darkTokens() : lightTokens();
    QColor onSurface           = token("onSurface");
    QColor primary             = token("primary");
    const QList<QPair<QString, QString>> layered = {
        { "primary", "onPrimary" },
        { "secondaryContainer", "onSecondaryContainer" },
        { "surface", "onSurface" },
        { "surfaceContainerLow", "onSurface" },
        { "surfaceContainer", "onSurface" },
        { "surfaceContainerHigh", "onSurface" },
        { "surfaceContainerHighest", "onSurface" },
    };
    for (auto &l : layered) {
        QColor b = token(l.first), o = token(l.second);
        tk[l.first + "_hover"]   = blend(b, o, 0.08).name();
        tk[l.first + "_pressed"] = blend(b, o, 0.12).name();
    }
    for (const QString &s : { QString("success"), QString("danger") }) {
        QColor b           = token(s);
        tk[s + "_hover"]   = blend(b, Qt::white, 0.08).name();
        tk[s + "_pressed"] = blend(b, Qt::black, 0.12).name();
    }
    tk["hoverLayer"]          = rgba(onSurface, 0.08);
    tk["pressedLayer"]        = rgba(onSurface, 0.12);
    tk["primaryHoverLayer"]   = rgba(primary, 0.08);
    tk["primaryPressedLayer"] = rgba(primary, 0.12);
    tk["disabledText"]        = rgba(onSurface, 0.38);
    tk["disabledContainer"]   = rgba(onSurface, 0.12);

    QPalette p;
    p.setColor(QPalette::Window, token("surface"));
    p.setColor(QPalette::WindowText, onSurface);
    p.setColor(QPalette::Base, token("surfaceContainerLowest"));
    p.setColor(QPalette::AlternateBase, token("surfaceContainerLow"));
    p.setColor(QPalette::Text, onSurface);
    p.setColor(QPalette::Button, token("surfaceContainer"));
    p.setColor(QPalette::ButtonText, onSurface);
    p.setColor(QPalette::ToolTipBase, token("inverseSurface"));
    p.setColor(QPalette::ToolTipText, token("inverseOnSurface"));
    p.setColor(QPalette::Highlight, token("primaryContainer"));
    p.setColor(QPalette::HighlightedText, token("onPrimaryContainer"));
    p.setColor(QPalette::Link, primary);
    p.setColor(QPalette::PlaceholderText, token("onSurfaceVariant"));
    p.setColor(QPalette::Mid, token("outlineVariant"));
    p.setColor(QPalette::Light, token("surfaceContainerHighest"));
    p.setColor(QPalette::Dark, token("outline"));
    QColor dis = blend(token("surface"), onSurface, 0.38);
    p.setColor(QPalette::Disabled, QPalette::Text, dis);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
    p.setColor(QPalette::Disabled, QPalette::WindowText, dis);
    qApp->setPalette(p);

    QFile file(":/themes/material3.qss");
    if (file.open(QFile::ReadOnly)) {
        QString qss = QString::fromUtf8(file.readAll());

        static const QRegularExpression iconRe(
            "@icon\\(\\s*(\\w+)\\s*,\\s*(\\w+)\\s*\\)");
        QRegularExpressionMatch m;
        while ((m = iconRe.match(qss)).hasMatch()) {
            QColor c(tk.value(m.captured(2), "#FF00FF"));
            qss.replace(m.capturedStart(), m.capturedLength(),
                        tintedSvgUrl(m.captured(1), c));
        }
        static const QRegularExpression tokRe("\\$\\{(\\w+)\\}");
        while ((m = tokRe.match(qss)).hasMatch())
            qss.replace(m.capturedStart(), m.capturedLength(),
                        tk.value(m.captured(1), "#FF00FF"));

        qApp->setStyleSheet(qss);
    }

    m_applied = true;
    emit themeChanged(dark);
}

QColor ThemeManager::canvasBackground() const
{
    return isDark() ? QColor("#17181B") : QColor("#F4F3F8");
}
QColor ThemeManager::gridDot() const
{
    return isDark() ? QColor("#3A3D44") : QColor("#C4C6D0");
}
QColor ThemeManager::wireColor() const
{
    return isDark() ? QColor("#C4C6D0") : QColor(40, 40, 60);
}
QColor ThemeManager::wireSelected() const
{
    return token("primary");
}
QColor ThemeManager::textColor() const
{
    return token("onSurface");
}
QColor ThemeManager::selectionColor() const
{
    return token("primary");
}
QColor ThemeManager::accentColor() const
{
    return token("primary");
}

QColor ThemeManager::iconColor() const
{
    return token("onSurfaceVariant");
}
QColor ThemeManager::iconDisabledColor() const
{
    QColor c = token("onSurface");
    c.setAlpha(97);
    return c;
}

// ---- Theme aware SVG icon engine ---------------------------------------
class MaterialIconEngine : public QIconEngine
{
  public:
    MaterialIconEngine(const QString &name, const QColor &fixed) :
        m_name(name), m_fixed(fixed)
    {
    }

    QIconEngine *clone() const override
    {
        return new MaterialIconEngine(m_name, m_fixed);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State) override
    {
        return render(size, 1.0, mode);
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
               QIcon::State) override
    {
        qreal dpr =
            painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        // Always draw a square glyph centred in the target rectangle so
        // non-square rects (tabs, line edits) never stretch or clip it.
        int side = qMin(rect.width(), rect.height());
        QRect sq(rect.x() + (rect.width() - side) / 2,
                 rect.y() + (rect.height() - side) / 2, side, side);
        painter->drawPixmap(sq, render(QSize(side, side) * dpr, dpr, mode));
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override
    {
        int side = qMin(size.width(), size.height());
        return QSize(side, side);
    }

    QList<QSize> availableSizes(QIcon::Mode, QIcon::State) const override
    {
        return { QSize(16, 16), QSize(18, 18), QSize(20, 20),
                 QSize(24, 24), QSize(32, 32), QSize(48, 48) };
    }

  private:
    QPixmap render(QSize size, qreal scale, QIcon::Mode mode)
    {
        int side = qMin(size.width(), size.height());
        if (side <= 0)
            return QPixmap();
        size             = QSize(side, side);
        ThemeManager *tm = ThemeManager::self();
        QColor c         = m_fixed.isValid() ? m_fixed : tm->iconColor();
        if (mode == QIcon::Disabled)
            c = m_fixed.isValid() ? QColor(c.red(), c.green(), c.blue(), 97)
                                  : tm->iconDisabledColor();
        else if (mode == QIcon::Selected || mode == QIcon::Active) {
            if (!m_fixed.isValid())
                c = tm->token("onSurface");
        }

        QString key = m_name + "|" + c.name(QColor::HexArgb) + "|" +
                      QString::number(side) + "@" + QString::number(scale);
        static QHash<QString, QPixmap> cache;
        auto it = cache.constFind(key);
        if (it != cache.constEnd())
            return *it;

        QSvgRenderer svg(QString(":/material/%1.svg").arg(m_name));
        QImage img(size, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        if (svg.isValid()) {
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            // Material Symbols already include a 2/24 optical margin, so
            // the glyph is rendered to the full square.
            svg.render(&p, QRectF(0, 0, side, side));
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(img.rect(), c);
        }
        QPixmap pm = QPixmap::fromImage(img);
        pm.setDevicePixelRatio(scale);
        cache.insert(key, pm);
        return pm;
    }

    QString m_name;
    QColor m_fixed;
};

QIcon ThemeManager::icon(const QString &name, const QColor &fixed)
{
    return QIcon(new MaterialIconEngine(name, fixed));
}

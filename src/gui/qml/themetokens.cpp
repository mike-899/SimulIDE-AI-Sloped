/***************************************************************************
 *   Material 3 theme tokens for the QML shell                             *
 *   GPL v3 or later                                                       *
 ***************************************************************************/
#include "themetokens.h"

#include "thememanager.h"

static const char *ColorRoles[] = { "primary",
                                    "onPrimary",
                                    "primaryContainer",
                                    "onPrimaryContainer",
                                    "secondaryContainer",
                                    "onSecondaryContainer",
                                    "tertiary",
                                    "onTertiary",
                                    "tertiaryContainer",
                                    "onTertiaryContainer",
                                    "surface",
                                    "surfaceContainerLowest",
                                    "surfaceContainerLow",
                                    "surfaceContainer",
                                    "surfaceContainerHigh",
                                    "surfaceContainerHighest",
                                    "onSurface",
                                    "onSurfaceVariant",
                                    "outline",
                                    "outlineVariant",
                                    "inverseSurface",
                                    "inverseOnSurface",
                                    "success" };

ThemeTokens::ThemeTokens(QObject *parent) : QQmlPropertyMap(this, parent)
{
    // Shape scale (docs/M3_EXPRESSIVE.md)
    insert("shapeXs", 4);
    insert("shapeS", 8);
    insert("shapeM", 12);
    insert("shapeL", 16);
    insert("shapeLI", 20);
    insert("shapeXL", 28);
    insert("shapeXLI", 32);
    insert("shapeXXL", 48);
    insert("fontFamily", QString("Inter"));

    refresh();
    connect(ThemeManager::self(), &ThemeManager::themeChanged, this, [this]() {
        refresh();
        emit darkChanged();
    });
}

void ThemeTokens::refresh()
{
    ThemeManager *tm = ThemeManager::self();
    bool dark        = tm->isDark();
    for (const char *role : ColorRoles)
        insert(role, tm->token(role));
    insert("error", tm->token("danger"));
    insert("onError", QColor(Qt::white));
    insert("errorContainer", dark ? QColor("#93000A") : QColor("#FFDAD6"));
    insert("onErrorContainer", dark ? QColor("#FFDAD6") : QColor("#410002"));
    insert("canvas", tm->canvasBackground());
    insert("shadow", QColor(0, 0, 0, dark ? 120 : 50));
    insert("isDark", dark);
}

bool ThemeTokens::dark() const
{
    return ThemeManager::self()->isDark();
}

void ThemeTokens::setDark(bool dark)
{
    ThemeManager::self()->setDark(dark);
}

void ThemeTokens::toggle()
{
    ThemeManager::self()->toggle();
}

QColor ThemeTokens::alpha(const QColor &c, qreal a) const
{
    QColor out = c;
    out.setAlphaF(a);
    return out;
}

#include "moc_themetokens.cpp"

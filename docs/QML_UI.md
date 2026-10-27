# Interfaz QML (Qt Quick)

*English summary at the end.*

La interfaz de SimulIDE es **híbrida**: el lienzo del circuito (`CircuitWidget`
/ `QGraphicsView`) y el editor de código siguen siendo QWidgets, y todo lo
demás (barra superior, panel de componentes, selector de páginas, cabecera del
panel de código, barra de edición flotante e inspector de propiedades) son
`QQuickWidget` con QML Material 3 Expressive (`docs/M3_EXPRESSIVE.md`).

## Arquitectura

```
src/qml/*.qml            páginas (una por QQuickWidget)
src/qml/controls/*.qml   controles reutilizables (Txt, Icon, IconButton, Tip, ...)
src/qml/qml.qrc          recursos (compilados dentro del binario, qrc:/qml/)
src/gui/qml/             puentes C++ (QmlUi, controladores, modelos)
src/mainwindow.cpp       createQmlChrome(): crea y coloca los QQuickWidget
```

* `QmlUi` (singleton) crea **un solo `QQmlEngine`** compartido por todos los
  QQuickWidget y registra los *context objects* y los proveedores de imagen
  `image://parts/<nombre>` (iconos de componentes) e `image://icon/<nombre>/<rrggbb>`
  (Material Symbols teñidos).
* `QmlUi::createWidget(archivo, padre, overlay)` carga `qrc:/qml/<archivo>`. Los
  *overlays* (barra de edición, inspector) tienen fondo transparente y se
  dimensionan al item raíz; `MainWindow::placeOverlays()` los coloca sobre el
  lienzo.
* Los QWidgets clásicos que aún hacen falta (editor, tabla RAM, explorador de
  archivos, monitor serie, diálogos) conviven con QML; el `ComponentSelector`
  sigue siendo el modelo de datos del panel de componentes (oculto).
* El popup nativo (`QMenu`, `QToolTip`) se usa para menús y tooltips para que
  no los recorte el QQuickWidget que los aloja (`Shell.popupMenu`,
  `Shell.popupChoice`, `Shell.showToolTip`).

### Context objects

| Objeto | Clase C++ | Contenido |
|---|---|---|
| `Theme` | `ThemeTokens` | Roles de color M3 (`Theme.primary`, `Theme.surfaceContainerHigh`, ...), escala de formas, `Theme.dark`, `Theme.alpha(color, a)`, `Theme.toggle()` |
| `Shell` | `ShellController` | `componentsVisible`, `codeVisible`, `sidePage`, `realisticParts`, `spice`, `version`, `actions` (QActions del circuito); `popupMenu()`, `popupChoice()`, `showToolTip()`, `hideToolTip()`, `toggleTheme()` |
| `Sim` | `SimController` | `state` (`Off`/`On`/`Paused`/`Debug`), `running`, `paused`, `canToggle`, `canPause`, `speed`, `error`; `toggle()`, `start()`, `stop()`, `pause()` |
| `Files` | `FileController` | `title`, `modified`; `newCircuit()`, `open()`, `save()`, `saveAs()`, `openCode()`, `newCode()` |
| `Inspector` | `Inspector` | `active`, `itemId`, `itemType`, `help`, `collapsed`, `model` (`PropertyListModel`) |
| `Components` | `ComponentModel` | secciones/tarjetas, `categories`, `category`, `search`; `startDrag(nombre)`, `manage()` |
| `Mcus` | `McuModel` | `mcus`, `currentIndex`, `empty`; `select(i)` |

`PropertyListModel` expone las propiedades `USER` del componente seleccionado
(mismo orden que el editor clásico). Roles: `name`, `label`, `value`, `display`,
`kind` (`bool`, `int`, `double`, **`quantity`**, `string`, `enum`, `color`,
`other`), `options`, `readOnly`. Una propiedad numérica seguida de su `Unit`
se fusiona en una sola fila `quantity` ("2.2 kΩ"): se acepta `4.7k`, `10 m`,
`2,2 kΩ`, `100`.

### Arrastrar y soltar

Una tarjeta del panel de componentes llama a `Components.startDrag(nombre)`,
que lanza un `QDrag` real (`ComponentSelector::execDrag`) con el mime que
espera `CircuitView::dropEvent`; el componente se crea en el punto de soltado.

## Nombres de objeto para pruebas (`objectName`)

QML: `menuButton`, `appTitle`, `projectName`, `themeToggleButton`,
`newCircuitButton`, `openCircuitButton`, `saveCircuitButton`,
`componentsButton`, `codeButton`, `simButton`, `componentSearch`,
`componentCategoryBox`, `componentGrid`, `manageComponentsButton`,
`pauseButton`, `simSpeedLabel`, `propertiesPanel`, `propertyInspectorTitle`,
`propertyInspectorCollapse`, `codePanelTitle`, `mcuSelector`, `codePanelClose`,
`codeEmptyState`, `codeOpenFileButton`, `codeNewFileButton`.

QWidgets que los alojan: `topBar`, `sidePanelHeader`, `componentLibrary`,
`codePanelHeader`, `editToolBar`, `propertyInspector`, `codeEmptyState`,
`sidePanel`, `sidePanelHost`, `codePanel`, `editor`, `serialMonitorToggle`,
`serialMonitorPanel`, `ramTabWidget`, `fileExplorer`, `mainMenu`, `circuit`,
`widgetsUiAction`.

## Ejecutar y probar

Requisitos de ejecución (módulos QML de Qt 6): `QtQuick`, `QtQuick.Controls`
(estilo Basic), `QtQuick.Layouts`, `QtQuick.Window`, `QtQuick.Templates`,
`QtQuick.Shapes`. En Debian/Ubuntu: `qml6-module-qtquick{,-controls,-layouts,-window,-templates,-shapes}`.

Sin GPU (Xvfb, CI, VM) usa el *software renderer*:

```sh
QT_QUICK_BACKEND=software xvfb-run -a -s "-screen 0 1600x900x24" \
    simulide --mcp-port 7991
```

* `SIMULIDE_SOFTWARE_RENDER=1` fuerza el renderizador por software. Si no se
  puede crear un contexto OpenGL, SimulIDE lo activa solo (`main.cpp`). Si
  defines `QT_QUICK_BACKEND`/`QSG_RHI_BACKEND`, se respeta tu elección.
* `SIMULIDE_QML_DIR=<ruta a src/qml>` carga el QML desde disco (edición en
  caliente: reinicia el programa tras cada cambio).
* Para pruebas automáticas usa el servidor MCP (`docs/MCP.md`) para crear el
  circuito y leer el estado (`get_component_state`) y `xdotool` para clics y
  arrastres reales. Tras `mousemove` espera ~0,4 s antes de `click`: los
  QQuickWidget necesitan el evento de *hover* previo.
* Sin gestor de ventanas el tamaño inicial es el 80 % de la pantalla
  (mínimo 1280x720) si no hay geometría guardada.

## Interfaz clásica (fallback)

* `--widgets-ui` (o `SIMULIDE_UI=widgets`) arranca la interfaz de QWidgets.
  `--qml-ui` fuerza QML.
* Ajuste `ui/qml=false` en `simulide.ini` (menú *Herramientas > Classic Widgets
  interface*, efectivo al reiniciar).
* Automático: antes de crear la ventana, `QmlUi::preflight()` compila los
  archivos QML raíz en un motor temporal; si falta un módulo de Qt Quick o un
  archivo no compila, se registra el error y se usa la interfaz de QWidgets.

## English summary

The shell is hybrid: the circuit canvas and code editor stay QWidgets; the top
bar, component library, side-panel header, code-panel header, floating edit
toolbar and property inspector are `QQuickWidget`s sharing one `QQmlEngine`
(`QmlUi`). C++ bridges are exposed as the context objects `Theme`, `Shell`,
`Sim`, `Files`, `Inspector`, `Components` and `Mcus` (table above). Run
headless with `QT_QUICK_BACKEND=software`; the software renderer is also chosen
automatically when OpenGL is unavailable (`SIMULIDE_SOFTWARE_RENDER=1` forces
it). `--widgets-ui` / setting `ui/qml=false` / a failed QML preflight select the
classic Widgets UI.

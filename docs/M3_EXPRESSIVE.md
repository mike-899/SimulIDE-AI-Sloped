# Material 3 Expressive: tokens de diseño

Referencia para la interfaz QML. Los valores de color son una paleta propia
(primario índigo, terciario magenta), no generada con la herramienta oficial.

## Paleta

| Rol | Claro | Oscuro |
|---|---|---|
| primary | #3F4FE0 | #BBC3FF |
| onPrimary | #FFFFFF | #13208F |
| primaryContainer | #DDE1FF | #2E3BC4 |
| onPrimaryContainer | #000C62 | #E0E3FF |
| secondaryContainer | #DCE0FF | #3A4170 |
| onSecondaryContainer | #151B3F | #DFE1FF |
| tertiary | #9C2D8C | #FFACE8 |
| onTertiary | #FFFFFF | #5C0053 |
| tertiaryContainer | #FFD7F1 | #7E1772 |
| onTertiaryContainer | #3A0033 | #FFD7F1 |
| surface | #FBF8FF | #11121C |
| surfaceContainerLowest | #FFFFFF | #0C0D16 |
| surfaceContainerLow | #F5F2FD | #191A28 |
| surfaceContainer | #EFECFA | #1E1F2E |
| surfaceContainerHigh | #E9E6F6 | #282A3B |
| surfaceContainerHighest | #E3E0F1 | #333547 |
| onSurface | #1A1B26 | #E4E1F4 |
| onSurfaceVariant | #454659 | #C6C5DC |
| outline | #75768B | #9090A6 |
| outlineVariant | #C6C5DC | #454659 |
| inverseSurface | #2F303C | #E4E1F4 |
| inverseOnSurface | #F2EFFD | #2F303C |
| success | #00855A | #1F9D6C |
| error | #BA1A1A | #D93636 |

Capas de estado: hover 8 %, focus/pressed 10-12 %, drag 16 %.
Deshabilitado: contenido onSurface 38 %, contenedor 12 %.

## Formas (radio de esquina)

none 0 · extra-small 4 · small 8 · medium 12 · large 16 · large-increased 20 ·
extra-large 28 · extra-large-increased 32 · extra-extra-large 48 · full = alto/2.

- Botones: totalmente redondos en reposo; al presionar se vuelven más
  cuadrados (8-12). Toggle seleccionado: de redondo a cuadrado (o al revés en
  botones de ícono).
- Grupo de botones conectado: esquinas internas 8 (4 en tamaño chico),
  externas redondas; el elemento seleccionado pasa a redondo completo.
- FAB 16, FAB mediano 20, FAB grande 28; FAB extendido de 56 de alto, radio 16.
- Tarjetas 12-16, diálogos y hojas 28, barra flotante redonda completa
  (64 de alto), chips 8, buscador redondo completo de 56.
- Biblioteca de 35 formas (círculo, cookie de 4/6/8/9/12 lados, trébol,
  burst, flor, puffy, etc.) para avatares, contenedores de íconos, menú del FAB
  e indicador de carga; las transiciones entre formas se animan con morph.

## Tipografía

Inter (incluida en `resources/fonts`); la especificación usa Roboto Flex.

| Estilo | L | M | S |
|---|---|---|---|
| display | 57/64 | 45/52 | 36/44 |
| headline | 32/40 | 28/36 | 24/32 |
| title | 22/28 | 16/24 | 14/20 |
| body | 16/24 | 14/20 | 12/16 |
| label | 14/20 | 12/16 | 11/16 |

Peso 400 en los estilos normales y 500 en title M/S y labels. Las variantes
enfatizadas suben un escalón (400→500, 500→600/700) y se usan en títulos de
app y paneles, estados seleccionados y acciones primarias. Para títulos
destacados: tamaño mayor, peso 700-800 y tracking levemente negativo.

## Movimiento (resortes)

| Esquema | Espacial | Efectos (color/opacidad, sin rebote) |
|---|---|---|
| Expressive rápido | damping 0.6, stiffness 800 | damping 1.0, stiffness 3800 |
| Expressive normal | damping 0.8, stiffness 380 | damping 1.0, stiffness 1600 |
| Expressive lento | damping 0.8, stiffness 200 | damping 1.0, stiffness 800 |
| Estándar (menos rebote) | damping 0.9, stiffness 700 | — |

Aproximaciones con curvas:
- Movimiento espacial: OutBack (overshoot ~1.2-1.7), 250-350 ms.
- Entrada: emphasized decelerate `cubic-bezier(0.05, 0.7, 0.1, 1.0)`, 400 ms.
- Salida: emphasized accelerate `cubic-bezier(0.3, 0, 0.8, 0.15)`, 200 ms.

En QML: `SpringAnimation { spring: 3-5; damping: 0.2-0.3 }` o `Easing.OutBack`.

## Componentes

- Split button: parte inicial y final con 2 px de separación, esquinas
  internas de 4.
- Menú del FAB: despliega 2-6 ítems en píldora; el FAB pasa a ser el botón de
  cerrar.
- Barras de herramientas: fijas o flotantes, con variante vibrante
  (primaryContainer).
- Indicador de carga: ícono de forma cambiante, opcionalmente sobre un círculo.
- Indicadores de progreso: variantes onduladas lineal y circular (amplitud
  ~3 px, longitud de onda ~40 px).
- Pestañas y segmentos seleccionados: forma de píldora.

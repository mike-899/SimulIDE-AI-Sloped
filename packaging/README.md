# Empaquetado de SimulIDE-AI-Sloped

Contenido:

- `.github/workflows/build.yaml`: compila en Linux (AppImage + tar.gz) y Windows (instalador + zip portable).
- `packaging/windows/simulide.nsi`: script NSIS del instalador de Windows.
- `packaging/linux/simulide.desktop`: lanzador de escritorio (AppImage y Arch).
- `packaging/arch/PKGBUILD` y `.SRCINFO`: paquete `simulide-ai-sloped-git` para Arch Linux.

La interfaz QML (`src/qml`, ver `docs/QML_UI.md`) va compilada en el binario (qrc) pero necesita los modulos QML de Qt en tiempo de ejecucion: `qml6-module-qtquick*` en Ubuntu (AppImage: `QML_SOURCES_PATHS=src/qml` para linuxdeploy-plugin-qt), `mingw-w64-x86_64-qt6-declarative` + `windeployqt6 --qmldir src/qml` en Windows y `qt6-declarative` en Arch.

Todo se compila con **Qt 6** (`qmake6`): Ubuntu 24.04 con los paquetes `qt6-*-dev`, MSYS2 con `mingw-w64-x86_64-qt6-*` (y `windeployqt6`) y Arch con `qt6-base qt6-multimedia qt6-serialport qt6-svg qt6-declarative`.

## Windows: obtener el instalador

### Desde GitHub Actions (cualquier commit)

1. Entra en la pestana **Actions** del repositorio y abre la ejecucion "Build" que quieras (o lanzala a mano con **Run workflow**).
2. Al final de la pagina, en **Artifacts**, descarga `simulide-windows`.
3. Dentro del zip hay:
   - `SimulIDE-<version>-win64-setup.exe`: instalador (menu Inicio, acceso directo en el escritorio, desinstalador).
   - `SimulIDE-<version>-win64-portable.zip`: version portable; descomprime y ejecuta `bin\simulide.exe`.

### Desde Releases (versiones publicadas)

Al subir una etiqueta que empiece por `v`, el workflow crea una Release con todos los archivos adjuntos (instalador, zip, AppImage y tar.gz):

```bash
git tag v0.4.13-1
git push origin v0.4.13-1
```

Descargalos despues desde la pestana **Releases**.

Nota: el instalador no esta firmado, asi que Windows SmartScreen puede mostrar un aviso ("Mas informacion" y "Ejecutar de todas formas").

## Arch Linux: instalar con makepkg

```bash
git clone https://github.com/mike-899/SimulIDE-AI-Sloped.git
cd SimulIDE-AI-Sloped/packaging/arch
makepkg -si
```

Esto instala las dependencias, compila y deja el programa listo. Se ejecuta con `simulide-ai-sloped` (tambien existe el alias `simulide`).

Detalle de la instalacion: SimulIDE busca sus datos en `<directorio del ejecutable>/../share/simulide`, asi que el arbol completo `bin/` + `share/` se instala en `/usr/lib/simulide-ai-sloped/` y `/usr/bin/simulide-ai-sloped` es un enlace simbolico (Qt resuelve el enlace, por lo que los datos se encuentran). `/usr/share/simulide-ai-sloped` apunta a los datos. El paquete entra en conflicto con el paquete `simulide` oficial.

## Publicar en el AUR (`yay -S simulide-ai-sloped-git`)

Necesitas una cuenta en https://aur.archlinux.org y una clave SSH.

1. **Cuenta**: registrate en https://aur.archlinux.org/register.
2. **Clave SSH**: genera una (`ssh-keygen -t ed25519 -f ~/.ssh/aur`) y pega el contenido de `~/.ssh/aur.pub` en *My Account > SSH Public Key* del AUR. Anade a `~/.ssh/config`:
   ```
   Host aur.archlinux.org
       IdentityFile ~/.ssh/aur
       User aur
   ```
3. **Clona el repositorio vacio del paquete** (se crea al hacer el primer push):
   ```bash
   git clone ssh://aur@aur.archlinux.org/simulide-ai-sloped-git.git aur-simulide
   ```
4. **Copia los archivos y regenera `.SRCINFO`** (en un Arch):
   ```bash
   cp SimulIDE-AI-Sloped/packaging/arch/PKGBUILD aur-simulide/
   cd aur-simulide
   makepkg --printsrcinfo > .SRCINFO
   ```
5. **Prueba** que compila limpio: `makepkg -sf` (opcional: `namcap PKGBUILD`).
6. **Sube**:
   ```bash
   git add PKGBUILD .SRCINFO
   git commit -m "Initial import"
   git push origin master
   ```
7. Listo: ya se puede instalar con `yay -S simulide-ai-sloped-git` (o `paru`).

Para actualizar el empaquetado (no hace falta para nuevas versiones del codigo, porque es un paquete `-git` y `pkgver()` toma la version del repositorio): edita el PKGBUILD, regenera `.SRCINFO`, commit y push.

Importante: el PKGBUILD clona el repositorio publico de GitHub y usa `packaging/linux/simulide.desktop` de ese repositorio, asi que estos archivos deben estar en la rama por defecto antes de publicar en el AUR.

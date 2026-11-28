# Instalación automática de STEMS

1. Descomprime el ZIP.
2. Abre `Instalar STEMS.command`.
3. La primera instalación requiere Internet. El instalador descarga Python 3.8.10 desde python.org, prepara un entorno virtual y descarga Spleeter y su modelo 4stems.
4. Se requiere FFmpeg instalado y disponible en PATH. Si no está presente, el instalador se detiene y lo indica; no activa STEMS incompleto.
5. Tras instalar correctamente, la separación y la reproducción de STEMS se realizan localmente y no necesitan Internet.

Compatibilidad objetivo: macOS 10.13.6 Intel. Python/TensorFlow/Spleeter son dependencias antiguas; la instalación debe probarse en el equipo real. El instalador puede solicitar autorización de macOS para instalar el paquete oficial de Python.

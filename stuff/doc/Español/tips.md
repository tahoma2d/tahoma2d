# Bienvenidos a Tahoma2D 1.5!

La versión 1.5 trae una serie de nuevas características, mejoras y aún más correcciones de errores, que se encuentran listadas [aquí](https://github.com/tahoma2d/tahoma2d/releases/tag/v1.5).

### Compatibility Changes

Palettes with Raster based custom styles (`Style Editor` -> `Vector` tab -> `Custom styles`) will be auto upgraded to new format which will preventing use in prior versions of T2D if the paleette is saved in the latest T2D version.

### Usuarios de Windows

En caso de que se experimentaran problemas con la pluma en esta nueva versión, especialmente al usar una tableta-laptop/tableta Surface, intentar una o ambas de las siguientes alternativas:
- Activar `Preferencias` -> `Opciones táctiles/tableta` -> `Usar soporte nativo de Qt para Windows Ink*` y reiniciar Tahoma2D.
- Mantener activa la opción de arriba e instalar controladores de `WinTab`. Comprobar con el fabricante del dispositivo para obtener los controladores apropiados de WinTab para el mismo.

-----

## Actualización desde Tahoma2D 1.2 o migración desde OpenToonz

En caso de partir desde Tahoma2D 1.2 u OpenToonz, aquí se mostrarán algunos puntos a tener en cuenta:
- Cambios en la Línea de tiempo/Planilla
  - Las celdas y columnas ya no tienen barras de arrastre, de manera predefinida.
  - Hacer clic sobre una celda expuesta o una columna (que no esté vacía) para moverla.
  - Al seleccionar y arrastrar sobre una celda/columna vacía se iniciará una selección de múltiples celdas/columnas.
  - `ALT` ha reemplazado a `CTRL` para la selección de fotogramas y claves de animación.
  - `CTRL` ahora iniciará una selección de múltiples celdas, cuando el primer clic se realice sobre una celda/columna expuesta.
  - En caso de preferirse el comportamiento anterior, activar la opción `Preferencias` -> `Escena` -> `Mostrar barras de arrastre de celdas y columnas`.
- Mantenimiento implícito
  - Ya no habrá necesidad de extender un dibujo manualmente. Se mantendrá la exposición del mismo hacia adelante hasta el siguiente dibujo expuesto.
  - Usar un `Fotograma de detención del mantenimiento` para finalizar la exposición automática, sin necesidad de la aparición de otro dibujo expuesto.
  - Hacer clic derecho sobre cualquier celda y seleccionar `Mantenimiento implícito` para alternar entre ambos modos.
  - Será posible convertir una escena existente usando las opciones `Escena` -> `Convertir a mantenimiento implícito/explícito`.
- Algunas características y opciones de OpenToonz se encuentran ocultas de forma predefinida.
  - Será posible activarlas usando la opción `Preferencias` -> `General` -> `Mostrar preferencias y opciones avanzadas` y luego reiniciando el programa.

-----
## Consejos
- La animación de la escena y los recursos (los niveles de animación) son almacenados en archivos independientes, dentro de la carpeta del proyecto.
- Crear una carpeta `Tahoma2D` dentro de la carpeta Documentos y usarla para almacenar allí todos los proyectos, en vez de usar la carpeta predefinida `sandbox`.
- Usar el comando `Guardar todo` de manera regular, para guardar tanto la escena como los archivos de los recursos usados. El comando `Guardar escena` no guardará los cambios en los recursos, sino solamente los de la escena.
- Al usar recursos externos al proyecto, se recomienda utilizar la opción `Importar` para mantener los archivos originales intactos, ya que Tahoma2D usualmente modifica los archivos usados dentro de un proyecto.
- Para que una secuencia de archivos de imagen sea tomada por el programa como 1 único nivel, deberán usar esta nomenclatura `nombre_de_nivel.####.ext` o `nombre_de_nivel_####.ext`. (p.ej: ABC.0001.png, ABC.0002.png, etc...)
- En la Barra de estado se mostrará información adicional para muchas opciones, comandos y herramientas.
- En caso de necesitar ayuda, unirse al canal de Tahoma2D en Discord o en las otras redes sociales listadas en https://tahoma2d.org.
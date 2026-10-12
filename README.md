# Proyecto 6 – Factorización LU paralela con MPI

Métodos Numéricos II – Computación Científica · FACET-UNT · 2026
Autores: (completar)

En este proyecto resolvimos sistemas de ecuaciones A x = b con factorización LU con pivoteo parcial. Primero hicimos una versión en serie y después una en paralelo con MPI. Medimos tiempos, speedup y precisión en una notebook y en el clúster de Córdoba, con matrices de hasta 30000 × 30000 y hasta 96 procesos.

## Cómo funciona

La idea es la de siempre: factorizar A = L U, y después resolver L y = b (hacia adelante) y U x = y (hacia atrás).

Algunas decisiones que tomamos:

- **Pivoteo parcial sin mover filas.** En cada paso elegimos como pivote la fila con el número más grande (en valor absoluto) en la columna del paso, pero no la intercambiamos: anotamos cuál fue en `pivote_del_paso`. Así, en la versión MPI no hay que mandar filas enteras de un proceso a otro.
- **En MPI se reparten las filas.** Cada proceso guarda solo sus filas y las actualiza. Probamos dos repartos: **cíclico** (`c`, como repartir cartas) y **por bloques** (`b`, cada proceso recibe filas seguidas).
- **En cada paso de la factorización:**
  1. cada proceso busca su mejor candidato a pivote;
  2. con `MPI_Allreduce` y `MPI_MAXLOC` se elige el ganador entre todos;
  3. el dueño de esa fila la manda a todos con `MPI_Bcast`;
  4. cada uno actualiza sus filas.
- **La matriz de prueba** la genera `generar.c`: números al azar entre −1 y 1, con semilla fija, y b = A·(1,…,1). Así sabemos que la solución exacta es x = (1,…,1) y podemos medir el error.

## Archivos

```
codigo/        generar.c, lu_serial.c, lu_mpi.c
scripts/       medir.sh, medir_cluster.sh, medir_grande.sh
resultados/    los CSV con las mediciones, graficos.py y la carpeta figuras/
README.md
```

- `codigo/generar.c`: crea el sistema y lo guarda en `sistema_N.bin`.
- `codigo/lu_serial.c`: versión en serie.
- `codigo/lu_mpi.c`: versión MPI. Cíclico y bloques están en el mismo código.
- `scripts/medir*.sh`: corren todas las mediciones (PC, clúster y clúster con N grandes). Para usarlos hay que copiarlos a la carpeta donde están los programas compilados.
- `resultados/resultados_pc.csv`, `resultados_cluster.csv`, `resultados_cluster_grande.csv`: todas las mediciones, una línea por corrida.
- `resultados/experimento_ubicacion.csv`: el experimento de la caché (ver más abajo).
- `resultados/graficos.py`: arma los gráficos en `resultados/figuras/`.

## Cómo compilar y correr

```bash
cd codigo
gcc   -O3 generar.c   -o generar
gcc   -O3 lu_serial.c -o lu_serial -lm
mpicc -O3 lu_mpi.c    -o lu_mpi    -lm

./generar 4000
./lu_serial sistema_4000.bin
mpirun -n 16 --bind-to core ./lu_mpi sistema_4000.bin c     # c = cíclico, b = bloques

python3 ../resultados/graficos.py                           # gráficos en resultados/figuras/
```

Cada corrida imprime una línea con: `N, p, reparto, t_total, t_lectura, t_factorizar, t_comunicacion, t_calculo, t_juntar, t_sustitucion, residuo_abs, residuo_rel, error_x`.

## Cómo medimos

- **Notebook:** Intel i5-13450HX, con 6 núcleos rápidos y 4 lentos. Medimos N = 500 a 4000 y p = 1 a 10.
- **Clúster:** 2 procesadores AMD EPYC 7B12, con una cuota de 96 núcleos. Medimos N = 500 a 30000 y p = 1 a 96.
- **Repeticiones:** cada caso se repitió 10 veces (5 para N = 8000 y 2 para N ≥ 12000), y usamos la mediana. El serial de N = 30000 no lo corrimos porque tardaba más de una hora.
- **Qué tiempo usamos:** de cada tiempo tomamos el del proceso más lento, porque el programa termina cuando termina el último. El speedup lo calculamos con el tiempo de la factorización, que es la parte que paralelizamos.

## Qué encontramos

**El tiempo crece como N³.** Si se duplica N, tarda unas 8 veces más: en el clúster, N = 8000 tarda 100 s y N = 16000 tarda 769 s. Un núcleo del clúster es casi igual de rápido que uno de la notebook (N = 4000: 12,8 s contra 12,0 s). La ventaja del clúster es que tiene muchos.

**El speedup depende mucho de N.** Este fue el mejor resultado para cada tamaño en el clúster:

| N | mejor p | speedup |
|---|---|---|
| 500 | 8 | 2,7 |
| 1000 | 32 | 4,3 |
| 2000 | 32 | 16,0 |
| 4000 | 64 | 32,9 |
| 8000 | 96 | 29,6 |
| 12000 a 20000 | 96 | entre 10 y 12 |

Con matrices chicas, usar muchos procesos empeora las cosas. Con N = 500 y 96 procesos tarda más que el serial, porque en cada paso todos se tienen que poner de acuerdo, y eso cuesta más que las cuentas que se ahorran. La comunicación deja de pesar más que el cálculo cuando cada proceso tiene unas 100 filas.

**Los saltos del speedup se explican por la caché.** Algo que no esperábamos: el speedup no sube de a poco, sino que da saltos. Por ejemplo, N = 4000 pasa de 5,4 con 16 procesos a 26,8 con 32.

En el procesador del clúster, cada 4 núcleos comparten una caché L3 de 16 MB, así que a cada proceso le tocan unos 4 MB. El salto aparece justo cuando el pedazo de matriz de cada proceso (N·N·8/p bytes) baja a ese tamaño y entra en la caché.

Para comprobarlo hicimos dos cosas:
- **Corrimos N = 4000 con 16 procesos, cambiando solo dónde se ubican.** Con 4 procesos por caché tardó 2,53 s; con 1 proceso por caché (`--map-by ppr:1:l3cache`), 0,53 s. Con una matriz que no entra en la caché (N = 8000) la mejora fue bastante menor (2,6 veces), así que una parte es la caché y otra el acceso a memoria.
- **Medimos con `perf` de dónde traía los datos el procesador.** Con 4 procesos por caché, cerca del 30 % de los datos venía de la RAM; con 1 por caché, el 0,1 %. Esto es una sola corrida de cada caso.

Esto también explica que con N = 2000 y 4 u 8 procesos el speedup haya dado **más que p** (superlineal): la matriz entera no entra en la caché, pero repartida sí.

**La parte que no se paraleliza se nota (ley de Amdahl).** Con N = 4000 y 64 procesos, la factorización va 32,9 veces más rápido, pero el programa completo solo 20,2. Leer el archivo, repartir, juntar y la sustitución las hace un solo proceso, y con tantos procesos llegan a ser casi el 40 % del tiempo. Con matrices grandes eso pesa poco.

**Cíclico y bloques dan casi lo mismo.** La diferencia es de ±2 % en el clúster. Creemos que es porque, al no intercambiar filas y con una matriz al azar, las filas que ya fueron pivote quedan repartidas entre todos los procesos con cualquiera de los dos repartos.

**En la notebook, lo mejor fue usar 6 procesos.** Son los 6 núcleos rápidos: con 8 o 10 entran los núcleos lentos, y como todos esperan al más lento en cada paso, empeora. El speedup con 6 fue 6,3 para N = 1000, pero solo 1,4 para N = 4000, donde la matriz ya no entra en la caché y la memoria de la notebook se vuelve el límite.

**La precisión es buena y no cambia al paralelizar.** El residuo relativo ‖Ax − b‖/‖b‖ quedó entre 10⁻¹⁴ y 3·10⁻¹², o sea entre 12 y 14 dígitos correctos. Está por debajo de lo que se espera solo por el redondeo de la computadora (del orden de N·ε). Además da exactamente igual con cualquier cantidad de procesos, con los dos repartos y en las dos máquinas.

Los gráficos están en `resultados/figuras/`:

- `01_G1`: tiempo de factorización vs N (serial y paralelo).
- `02_G2`: tiempo de cálculo vs tiempo de comunicación, para p = 16, 32 y 96.
- `03_G3b`: speedup vs cantidad de procesos, en el clúster.
- `04_N4`: experimento de la caché (4 procesos por L3 contra 1 por L3).
- `05_E5` y `06_E5`: tiempo total para cada cantidad de procesos, en el clúster y en la notebook.
- `07_E4`: diferencia entre reparto cíclico y por bloques.
- `08_G6`: precisión (residuo) vs N.

## Qué existe ya que hace esto

Buscando si había programas que ya resolvieran este problema, encontramos estos:

- **LAPACK**: la biblioteca estándar de álgebra lineal (numpy, scipy y MATLAB la usan por dentro). Su función `dgetrf` hace la LU con pivoteo parcial, como nuestro `lu_serial.c`, pero **por bloques**: agrupa varios pasos y actualiza la matriz de a pedazos que entran en la caché, así va mucho menos a la RAM.
- **ScaLAPACK**: la versión de LAPACK para MPI (`pdgetrf`). En vez de repartir filas, como nosotros, reparte la matriz en bloques cuadrados sobre una grilla de procesos, lo que equilibra mejor el trabajo y la comunicación.
- **HPL (High-Performance Linpack)**: resuelve un sistema denso aleatorio con LU y pivoteo parcial usando MPI. Es casi lo mismo que hicimos nosotros, y es el programa con el que se arma el ranking TOP500 de supercomputadoras.

Nosotros programamos el algoritmo desde cero, porque esa era la consigna y queríamos entender qué pasa en cada paso. Estas bibliotecas sirven como referencia de lo que se puede mejorar.

## Mejoras que nos quedaron pendientes

- **Usar `MPI_Scatterv` para repartir en bloques.** Ahora P0 lee el archivo y manda las filas una por una, con `MPI_Send`. Para el reparto en bloques, `Scatterv` mandaría cada bloque de una sola vez, y probablemente sería más rápido. Con N = 4000 y 64 procesos, repartir es cerca del 15 % del tiempo total. (Para el reparto cíclico no sirve directo, porque las filas de cada proceso no están juntas.)
- **Leer en paralelo.** Que cada proceso lea sus propias filas directamente del archivo (con `fseek` o con MPI-IO), en vez de que P0 lea todo y reparta.
- **No juntar la matriz en P0.** Al final P0 vuelve a necesitar la matriz entera para la sustitución, así que el ahorro de memoria se pierde. Se podría hacer la sustitución repartida entre los procesos.
- **Ubicar mejor los procesos.** Vimos que poniendo un proceso por caché L3 (`--map-by ppr:1:l3cache`), con 16 procesos el speedup pasó de 5,4 a 24. Habría que medirlo para todos los casos.
- **Factorizar por bloques.** Trabajar con pedazos de la matriz que entren en la caché, en vez de fila por fila. Es lo que hacen las bibliotecas como LAPACK, y apunta justo al problema de memoria que encontramos.
- **Superponer comunicación y cálculo.** Mientras se actualizan las filas, ir buscando el pivote del paso siguiente (con mensajes no bloqueantes), para no esperar tanto en cada paso.
- **Calcular los errores en paralelo.** Ahora los calcula solo P0.
- **Tolerancia del pivote relativa.** Ahora es fija (10⁻¹²), lo que sirve porque los números son de orden 1; para otras matrices habría que usar una relativa al tamaño de los números.
- **Probar con varias máquinas del clúster.** Todo corrió en una sola máquina. Con varias, la comunicación sería por red y seguramente pesaría más.

# Comparación con LAPACK

Resolvimos los mismos sistemas (`sistema_N.bin`) con LAPACK desde Python (`scipy.linalg.lu_factor`, con OpenBLAS), usando `codigo/comparar_lapack.py`. Hicimos una sola corrida de cada caso, el 12/10/2026.

## 1. Precisión: del mismo orden

Residuo relativo ‖Ax − b‖ / ‖b‖:

| N | Nuestro | LAPACK |
|---|---|---|
| 1000 | 3,1e-14 | 1,7e-14 |
| 2000 | 7,3e-14 | 4,0e-14 |
| 4000 | 1,8e-13 | 9,5e-14 |
| 8000 | 4,7e-13 | 2,7e-13 |
| 16000 | 1,4e-12 | 6,8e-13 |

- Siempre da del mismo orden; LAPACK es unas 2 veces mejor.
- El `error_x` alto de N = 1000 también aparece en LAPACK, así que viene de esa matriz y no de nuestro programa.
- El nuestro da exactamente igual con cualquier p. El de LAPACK cambia un poco según la cantidad de hilos.

## 2. Velocidad en serie: LAPACK es ~13 veces más rápido

| N | Nuestro serial | LAPACK, 1 núcleo | LAPACK es… |
|---|---|---|---|
| 4000 (notebook) | 12,0 s | 0,89 s | 13,5× |
| 4000 (cluster) | 12,9 s | 1,07 s | 12× |
| 8000 (cluster) | 100 s | 7,8 s | 13× |
| 16000 (cluster) | 12,8 min | 59 s | 13× |

Hay dos motivos:

- **Factoriza por bloques** que entran en la caché, así que va mucho menos a la RAM. Es justo lo que limita a nuestro programa.
- **Usa una BLAS optimizada** para el procesador. Con la BLAS básica, sin optimizar, LAPACK tardaba 4,7 s en lugar de 0,89 s.

## 3. En paralelo, con la misma cantidad de núcleos (solo como referencia)

Nuestro programa usa MPI (procesos que se mandan mensajes) y LAPACK usa hilos (memoria compartida). En cada fila los dos usan los mismos núcleos.

| Máquina | Núcleos | N | Nuestro (MPI) | LAPACK (hilos) | LAPACK es… |
|---|---|---|---|---|---|
| Notebook | 6 | 4000 | 8,8 s | 0,23 s | 38× |
| Cluster | 32 | 4000 | 0,48 s | 0,32 s | 1,5× |
| Cluster | 32 | 8000 | 14,6 s | 1,27 s | 11× |
| Cluster | 32 | 16000 | 130 s | 4,8 s | 27× |

- En el cluster, con N = 4000 y 32 procesos, estamos cerca: cada proceso nuestro tiene 4 MB y eso entra en la caché.
- Con N más grande la diferencia crece, porque nuestro programa queda frenado por la memoria y LAPACK no.
- En esta comparación cambian dos cosas a la vez: el algoritmo (fila por fila contra bloques) y la forma de paralelizar (MPI contra hilos). Por eso no sirve para decir cuál de las dos formas de paralelizar es mejor.

## Conclusión

Nuestra precisión es la esperable: del mismo orden que la de LAPACK. En velocidad nos gana por factorizar por bloques y por usar una BLAS optimizada, y eso marca la mejora principal que le haríamos a nuestro programa.

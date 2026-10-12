/*
 * lu_mpi.c - Proyecto 6: factorizacion LU con pivoteo parcial, version MPI
 *
 * Resuelve A x = b:  1) A = L U   2) L y = b   3) U x = y
 * Las FILAS de A se reparten entre los p procesos; cada uno factoriza SUS filas.
 * Pivoteo parcial SIN intercambiar filas: se anota en pivote_del_paso que fila fue pivote.
 *
 * Compilar:  mpicc -O3 lu_mpi.c -o lu_mpi -lm
 * Ejecutar:  mpirun -n 4 ./lu_mpi sistema_1000.bin c      (c = ciclico, b = bloques)
 *
 * Salida (una linea CSV, la imprime P0; cada tiempo es el del proceso mas lento):
 *   N,p,reparto,t_total,t_lectura,t_factorizar,t_comunicacion,t_calculo,t_juntar,t_sustitucion,residuo_abs,residuo_rel,error_x
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>       // fabs, sqrt
#include <mpi.h>

// Un pivote con |valor| menor que esto se toma como 0 (matriz singular o casi).
// Del orden del error de redondeo acumulado (N * epsilon). Sirve para numeros de orden 1.
#define TOLERANCIA_PIVOTE 1e-12


// ---------- Variables globales del reparto (cada proceso tiene su propia copia) ----------
int  p;                 // cantidad de procesos
char reparto;           // 'c' = ciclico, 'b' = bloques
long filas_base;        // N / p: filas que tiene cada proceso como minimo (bloques)
long filas_resto;       // N % p: los primeros filas_resto procesos tienen una fila mas (bloques)


/* ---------- Funciones del reparto ----------
 *   dueno(fila)      -> que proceso tiene esa fila global
 *   local(fila)      -> en que lugar de su A_local la guarda su dueno
 *   global(rango, i) -> al reves: la fila local i del proceso 'rango', que fila global es
 *   inicio_bloque(q) -> (bloques) fila global donde empieza el bloque del proceso q
 * Ej ciclico N=5, p=2:   fila 0 1 2 3 4  ->  dueno 0 1 0 1 0,  local 0 0 1 1 2
 */
long inicio_bloque(int q) {
    if (q < filas_resto) {
        return q * (filas_base + 1);            // todos los anteriores tienen una fila extra
    } else {
        return q * filas_base + filas_resto;    // los anteriores: filas_base cada uno + las extras
    }
}

int dueno(long fila) {
    if (reparto == 'c') {
        return fila % p;
    } else {
        long limite = filas_resto * (filas_base + 1);   // hasta aca, bloques de filas_base + 1
        if (fila < limite) {
            return fila / (filas_base + 1);
        } else {
            return filas_resto + (fila - limite) / filas_base;   // despues, bloques de filas_base
        }
    }
}

long local(long fila) {
    if (reparto == 'c') {
        return fila / p;
    } else {
        return fila - inicio_bloque(dueno(fila));   // distancia al comienzo de su bloque
    }
}

long global(int rango, long i) {
    if (reparto == 'c') {
        return rango + i * p;
    } else {
        return inicio_bloque(rango) + i;
    }
}


/* ---------- Factorizar A = L U repartida ----------
 * Igual que el serial, pero cada proceso recorre solo SUS filas (A_local) y el pivote se
 * elige y se reparte con MPI. Al terminar, A_local tiene mis filas de L y U, y todos tienen
 * pivote_del_paso completo.
 * t_comunicacion: tiempo dentro de Allreduce + Bcast (incluye esperar a los demas).
 * Devuelve 0 si salio bien, 1 si encontro un pivote casi 0.
 */
int factorizar_lu_mpi(double *A_local, long N, long filas_locales, int rank,
                      long *pivote_del_paso, double *t_comunicacion) {

    int *ya_fue_pivote  = malloc(N * sizeof(int));     // casilla 'fila' = 1 si esa fila ya fue pivote
    double *fila_pivote = malloc(N * sizeof(double));  // copia de la fila pivote del paso

    for (long fila = 0; fila < N; fila++) {
        ya_fue_pivote[fila] = 0;
    }

    // Par (valor, fila) para MPI_MAXLOC: el orden double, int es el tipo MPI_DOUBLE_INT
    struct par_valor_fila {
        double valor;
        int fila;
    };
    struct par_valor_fila candidato;            // mi mejor fila
    struct par_valor_fila ganador;              // la mejor de todos

    *t_comunicacion = 0.0;

    for (long paso = 0; paso < N; paso++) {

        // (a) Mi candidato a pivote entre MIS filas que no fueron pivote.
        //     Arranca en -1: si no tengo candidatas, pierdo contra cualquiera.
        candidato.valor = -1.0;
        candidato.fila  = -1;
        for (long i = 0; i < filas_locales; i++) {          // i = lugar en A_local
            long fila = global(rank, i);                    // fila = numero global
            if (ya_fue_pivote[fila] == 0 && fabs(A_local[i*N + paso]) > candidato.valor) {
                candidato.valor = fabs(A_local[i*N + paso]);
                candidato.fila  = fila;
            }
        }

        // (b) El mayor de todos los candidatos; el resultado le llega a TODOS
        double t0 = MPI_Wtime();
        MPI_Allreduce(&candidato, &ganador, 1, MPI_DOUBLE_INT, MPI_MAXLOC, MPI_COMM_WORLD);
        *t_comunicacion += MPI_Wtime() - t0;

        long nro_fila_pivote = ganador.fila;
        ya_fue_pivote[nro_fila_pivote] = 1;     // todos lo anotan igual
        pivote_del_paso[paso] = nro_fila_pivote;

        // El dueno de la fila pivote la copia (desde la columna 'paso') y la manda a todos
        int dueno_pivote = dueno(nro_fila_pivote);
        if (rank == dueno_pivote) {
            long i_pivote = local(nro_fila_pivote);         // donde esta en MI A_local
            for (long col = paso; col < N; col++) {
                fila_pivote[col] = A_local[i_pivote*N + col];
            }
        }
        t0 = MPI_Wtime();
        MPI_Bcast(&fila_pivote[paso], N - paso, MPI_DOUBLE, dueno_pivote, MPI_COMM_WORLD);   // solo N - paso numeros
        *t_comunicacion += MPI_Wtime() - t0;

        // Pivote casi 0: todos lo ven a la vez (misma fila_pivote), asi que no hace falta MPI_Abort
        if (fabs(fila_pivote[paso]) < TOLERANCIA_PIVOTE) {
            if (rank == 0) {
                printf("Error: pivote casi 0 (%.3e) en el paso %ld: la matriz es singular o casi\n",
                       fila_pivote[paso], paso);
            }
            free(ya_fue_pivote);
            free(fila_pivote);
            return 1;
        }

        // (c) Actualizo MIS filas que no fueron pivote: fila = fila - multiplicador * fila pivote
        for (long i = 0; i < filas_locales; i++) {
            long fila = global(rank, i);
            if (ya_fue_pivote[fila] == 0) {
                double multiplicador = A_local[i*N + paso] / fila_pivote[paso];
                A_local[i*N + paso] = multiplicador;          // donde iria el 0 se guarda el multiplicador (L)
                for (long col = paso + 1; col < N; col++) {   // desde paso + 1 para no pisar el multiplicador
                    A_local[i*N + col] = A_local[i*N + col] - multiplicador * fila_pivote[col];
                }
            }
        }
    }

    free(ya_fue_pivote);
    free(fila_pivote);
    return 0;
}


/* ---------- Sustitucion (igual que el serial, solo P0) ----------
 * Como no se intercambiaron filas, la ecuacion del paso 'paso' esta en la fila
 * pivote_del_paso[paso] de A.
 */
void sustitucion(double *A, double *b, long *pivote_del_paso, double *x, long N) {

    double *y = malloc(N * sizeof(double));

    // L y = b (de arriba hacia abajo). La diagonal de L es 1: no se divide.
    for (long paso = 0; paso < N; paso++) {
        long fila = pivote_del_paso[paso];
        double valor = b[fila];
        for (long col = 0; col < paso; col++) {             // solo las y ya calculadas
            valor = valor - A[fila*N + col] * y[col];
        }
        y[paso] = valor;
    }

    // U x = y (de abajo hacia arriba)
    for (long paso = N - 1; paso >= 0; paso--) {            // long con signo: llega a -1 y termina
        long fila = pivote_del_paso[paso];
        double valor = y[paso];
        for (long col = paso + 1; col < N; col++) {
            valor = valor - A[fila*N + col] * x[col];
        }
        x[paso] = valor / A[fila*N + paso];                 // divido por la diagonal de U
    }

    free(y);
}


/* ---------- Errores (solo P0) ----------
 * La A original se vuelve a leer del archivo de a una fila (la de P0 ya es L y U).
 *   residuo_abs = ||Ax - b||
 *   residuo_rel = ||Ax - b|| / ||b||
 *   error_x     = ||x - (1,...,1)|| / raiz(N)      (la solucion exacta es (1,...,1))
 * Si no se puede abrir el archivo, devuelve -1 (un error real nunca es negativo).
 */
void calcular_errores(char *nombre_archivo, double *b, double *x, long N,
                      double *residuo_abs, double *residuo_rel, double *error_x) {

    FILE *archivo = fopen(nombre_archivo, "rb");
    if (archivo == NULL) {
        printf("Aviso: no se pudo volver a abrir %s para calcular los errores\n", nombre_archivo);
        *residuo_abs = -1.0;
        *residuo_rel = -1.0;
        *error_x     = -1.0;
        return;
    }

    double *fila_original = malloc(N * sizeof(double));   // una fila, se reutiliza

    long N_archivo;
    long leidos = fread(&N_archivo, sizeof(long), 1, archivo);   // salteo N

    double suma_residuo = 0.0;                  // suma de residuo^2
    double suma_b       = 0.0;                  // suma de b^2
    double suma_error   = 0.0;                  // suma de (x - 1)^2
    for (long fila = 0; fila < N; fila++) {
        leidos += fread(fila_original, sizeof(double), N, archivo);

        double residuo = -b[fila];                          // (fila de A) . x - b
        for (long col = 0; col < N; col++) {
            residuo = residuo + fila_original[col] * x[col];
        }

        suma_residuo = suma_residuo + residuo * residuo;
        suma_b       = suma_b + b[fila] * b[fila];
        suma_error   = suma_error + (x[fila] - 1.0) * (x[fila] - 1.0);
    }
    fclose(archivo);
    free(fila_original);

    if (leidos != 1 + N * N) {
        printf("Aviso: no se pudo releer bien %s para calcular los errores\n", nombre_archivo);
    }

    *residuo_abs = sqrt(suma_residuo);
    *residuo_rel = sqrt(suma_residuo) / sqrt(suma_b);
    *error_x     = sqrt(suma_error) / sqrt(N);
}


int main(int argc, char *argv[]) {

    // ---------- Arrancar MPI ----------
    int rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);       // mi numero: 0, 1, 2, ...
    MPI_Comm_size(MPI_COMM_WORLD, &p);          // cantidad de procesos (p es global)

    // ---------- Parametros: ./lu_mpi sistema_N.bin c|b ----------
    if (argc != 3 || (argv[2][0] != 'c' && argv[2][0] != 'b')) {
        if (rank == 0) {
            printf("Uso: mpirun -n P ./lu_mpi sistema_N.bin c|b   (c = ciclico, b = bloques)\n");
        }
        MPI_Finalize();
        return 1;
    }
    reparto = argv[2][0];

    MPI_Barrier(MPI_COMM_WORLD);                // todos arrancan juntos el cronometro
    double t_inicio = MPI_Wtime();

    // ================= 1) LECTURA Y REPARTO =================
    // N es long: N*N se pasa del limite de un int desde N = 46341
    long N = 0;
    FILE *archivo = NULL;
    if (rank == 0) {
        archivo = fopen(argv[1], "rb");
        // Estos errores los ve solo P0: MPI_Abort corta a todos (si no, los demas esperan para siempre)
        if (archivo == NULL) {
            printf("Error: no se pudo abrir %s\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        long leidos_N = fread(&N, sizeof(long), 1, archivo);
        if (leidos_N != 1 || N <= 0) {
            printf("Error: %s no tiene un N valido\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }

    MPI_Bcast(&N, 1, MPI_LONG, 0, MPI_COMM_WORLD);   // P0 le manda N a todos

    filas_base  = N / p;
    filas_resto = N % p;

    // Cada proceso cuenta sus filas con la misma regla (sin comunicacion)
    long filas_locales = 0;
    for (long fila = 0; fila < N; fila++) {
        if (dueno(fila) == rank) {
            filas_locales++;
        }
    }

    double *A_local = malloc(filas_locales * N * sizeof(double));   // solo MIS filas
    long *pivote_del_paso = malloc(N * sizeof(long));
    double *b = NULL;                           // solo P0 lo reserva (NULL en los demas: free no hace nada)
    if (rank == 0) {
        b = malloc(N * sizeof(double));
    }
    // filas_locales > 0: un proceso sin filas puede recibir NULL de malloc(0) sin que sea error
    if ((filas_locales > 0 && A_local == NULL) || pivote_del_paso == NULL || (rank == 0 && b == NULL)) {
        printf("Error: no hay memoria suficiente en el proceso %d\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (rank == 0) {
        // P0 lee el archivo fila por fila: la suya va directo a A_local, la de otro se la manda
        double *fila_aux = malloc(N * sizeof(double));   // una fila, se reutiliza (MPI_Send es bloqueante)
        long leidos = 0;
        for (long fila = 0; fila < N; fila++) {
            if (dueno(fila) == 0) {
                leidos += fread(&A_local[local(fila) * N], sizeof(double), N, archivo);
            } else {
                leidos += fread(fila_aux, sizeof(double), N, archivo);
                MPI_Send(fila_aux, N, MPI_DOUBLE, dueno(fila), 0, MPI_COMM_WORLD);   // etiqueta 0 = reparto
            }
        }
        leidos += fread(b, sizeof(double), N, archivo);   // despues de la matriz esta b
        fclose(archivo);
        free(fila_aux);
        if (leidos != N * N + N) {
            printf("Error: %s esta incompleto\n", argv[1]);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    } else {
        // Los mensajes de P0 llegan en el orden en que se mandaron (orden creciente de fila),
        // que es el orden de mis filas locales: la i-esima que llega va al lugar i
        for (long i = 0; i < filas_locales; i++) {
            MPI_Recv(&A_local[i * N], N, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    double t_fin_lectura = MPI_Wtime();

    // ================= 2) FACTORIZAR =================
    double t_comunicacion;

    MPI_Barrier(MPI_COMM_WORLD);                // P0 tardo mas en la lectura: arrancan juntos
    double t_inicio_factorizar = MPI_Wtime();

    if (factorizar_lu_mpi(A_local, N, filas_locales, rank, pivote_del_paso, &t_comunicacion) != 0) {
        MPI_Finalize();                         // pivote casi 0: todos terminan a la vez
        return 2;
    }

    double t_fin_factorizar = MPI_Wtime();

    // ================= 3) JUNTAR: P0 recibe todas las filas factorizadas =================
    // (P0 vuelve a necesitar la matriz entera: el ahorro de memoria se pierde al final)
    double *A = NULL;
    if (rank == 0) {
        A = malloc(N * N * sizeof(double));
        if (A == NULL) {
            printf("Error: no hay memoria suficiente en P0 para juntar la matriz\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        for (long fila = 0; fila < N; fila++) {
            if (dueno(fila) == 0) {
                for (long col = 0; col < N; col++) {          // A usa la fila GLOBAL, A_local el lugar LOCAL
                    A[fila*N + col] = A_local[local(fila)*N + col];
                }
            } else {
                MPI_Recv(&A[fila * N], N, MPI_DOUBLE, dueno(fila), 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);   // etiqueta 1 = juntar
            }
        }
    } else {
        for (long i = 0; i < filas_locales; i++) {            // en orden: P0 las pide en el mismo orden
            MPI_Send(&A_local[i * N], N, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD);
        }
    }

    double t_fin_juntar = MPI_Wtime();

    // ================= 4) SUSTITUCION y 5) ERRORES (solo P0) =================
    double *x = NULL;
    double residuo_abs = 0.0, residuo_rel = 0.0, error_x = 0.0;
    double t_fin_sustitucion = t_fin_juntar;    // en los demas, t_sustitucion queda en 0
    if (rank == 0) {
        x = malloc(N * sizeof(double));
        sustitucion(A, b, pivote_del_paso, x, N);         // es N^2 contra N^3 de factorizar: pesa poco
        t_fin_sustitucion = MPI_Wtime();

        calcular_errores(argv[1], b, x, N, &residuo_abs, &residuo_rel, &error_x);
    }

    double t_fin = MPI_Wtime();

    // ================= 6) TIEMPOS: el del proceso MAS LENTO =================
    // t_calculo se resta en cada proceso ANTES del Reduce (los maximos pueden ser de procesos distintos)
    double tiempos[7] = { t_fin             - t_inicio,              // 0: total
                          t_fin_lectura     - t_inicio,              // 1: lectura
                          t_fin_factorizar  - t_inicio_factorizar,   // 2: factorizar
                          t_comunicacion,                            // 3: comunicacion (dentro de factorizar)
                          (t_fin_factorizar - t_inicio_factorizar) - t_comunicacion,   // 4: calculo
                          t_fin_juntar      - t_fin_factorizar,      // 5: juntar
                          t_fin_sustitucion - t_fin_juntar };        // 6: sustitucion
    double maximos[7];
    MPI_Reduce(tiempos, maximos, 7, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);   // maximo casilla por casilla

    if (rank == 0) {
        if (N <= 5) {                           // N chico: mostrar x para comprobar a mano
            for (long i = 0; i < N; i++) {
                printf("x[%ld] = %.6f\n", i, x[i]);
            }
        }
        printf("%ld,%d,%c,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3e,%.3e,%.3e\n",
               N, p, reparto,
               maximos[0], maximos[1], maximos[2], maximos[3], maximos[4], maximos[5], maximos[6],
               residuo_abs, residuo_rel, error_x);
    }

    free(A_local);
    free(pivote_del_paso);
    free(b);                                    // en los procesos que no son P0 valen NULL: free no hace nada
    free(A);
    free(x);
    MPI_Finalize();
    return 0;
}

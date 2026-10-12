/*
 * lu_serial.c - Proyecto 6: factorizacion LU con pivoteo parcial, version SERIAL
 *
 * Resuelve A x = b:  1) A = L U   2) L y = b   3) U x = y
 * Pivoteo parcial SIN intercambiar filas: se anota en pivote_del_paso que fila fue pivote.
 * Hace las mismas cuentas que lu_mpi.c: con el mismo archivo dan los mismos errores.
 *
 * Compilar:  gcc -O3 lu_serial.c -o lu_serial -lm
 * Ejecutar:  ./lu_serial sistema_1000.bin
 *
 * Salida: una linea CSV con las mismas columnas que lu_mpi.c (p = 1, reparto = s):
 *   N,p,reparto,t_total,t_lectura,t_factorizar,t_comunicacion,t_calculo,t_juntar,t_sustitucion,residuo_abs,residuo_rel,error_x
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>       // fabs, sqrt
#include <time.h>       // clock_gettime

// Un pivote con |valor| menor que esto se toma como 0 (matriz singular o casi).
// Del orden del error de redondeo acumulado (N * epsilon). Sirve para numeros de orden 1.
#define TOLERANCIA_PIVOTE 1e-12


// Tiempo REAL en segundos (como MPI_Wtime), para que el speedup compare lo mismo
double tiempo_actual(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);       // reloj que solo avanza
    return t.tv_sec + t.tv_nsec / 1e9;        // segundos + nanosegundos (1e9 double: no division entera)
}


/* ---------- Factorizar A = L U (sobre la misma A) ----------
 * Al terminar, A tiene U y los multiplicadores de L (donde iban los ceros), y
 * pivote_del_paso[paso] dice que fila fue pivote en cada paso.
 * Devuelve 0 si salio bien, 1 si encontro un pivote casi 0.
 */
int factorizar_lu(double *A, long N, long *pivote_del_paso) {

    int *ya_fue_pivote = malloc(N * sizeof(int));      // casilla 'fila' = 1 si esa fila ya fue pivote
    double *fila_pivote = malloc(N * sizeof(double));  // copia de la fila pivote (igual que en MPI)

    for (long fila = 0; fila < N; fila++) {
        ya_fue_pivote[fila] = 0;
    }

    for (long paso = 0; paso < N; paso++) {

        // (a) Pivote: entre las filas que no fueron pivote, la del mayor |numero| en la columna 'paso'.
        //     Se recorren todas porque, al no intercambiar, pueden estar en cualquier lugar.
        long nro_fila_pivote = -1;
        double mejor_valor = -1.0;                      // pierde contra cualquiera
        for (long fila = 0; fila < N; fila++) {
            if (ya_fue_pivote[fila] == 0 && fabs(A[fila*N + paso]) > mejor_valor) {
                mejor_valor = fabs(A[fila*N + paso]);
                nro_fila_pivote = fila;
            }
        }

        // (b) Anoto la fila pivote y la copio desde la columna 'paso'
        ya_fue_pivote[nro_fila_pivote] = 1;
        pivote_del_paso[paso] = nro_fila_pivote;
        for (long col = paso; col < N; col++) {
            fila_pivote[col] = A[nro_fila_pivote*N + col];
        }

        if (fabs(fila_pivote[paso]) < TOLERANCIA_PIVOTE) {
            printf("Error: pivote casi 0 (%.3e) en el paso %ld: la matriz es singular o casi\n",
                   fila_pivote[paso], paso);
            free(ya_fue_pivote);
            free(fila_pivote);
            return 1;
        }

        // (c) Actualizo las filas que no fueron pivote: fila = fila - multiplicador * fila pivote
        for (long fila = 0; fila < N; fila++) {
            if (ya_fue_pivote[fila] == 0) {
                double multiplicador = A[fila*N + paso] / fila_pivote[paso];
                A[fila*N + paso] = multiplicador;             // donde iria el 0 se guarda el multiplicador (L)
                for (long col = paso + 1; col < N; col++) {   // desde paso + 1 para no pisar el multiplicador
                    A[fila*N + col] = A[fila*N + col] - multiplicador * fila_pivote[col];
                }
            }
        }
    }

    free(ya_fue_pivote);
    free(fila_pivote);
    return 0;
}


/* ---------- Sustitucion (identica a la de lu_mpi.c) ----------
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


/* ---------- Errores (con la A original, guardada antes de factorizar) ----------
 *   residuo_abs = ||Ax - b||
 *   residuo_rel = ||Ax - b|| / ||b||
 *   error_x     = ||x - (1,...,1)|| / raiz(N)      (la solucion exacta es (1,...,1))
 */
void calcular_errores(double *A_original, double *b, double *x, long N,
                      double *residuo_abs, double *residuo_rel, double *error_x) {

    double suma_residuo = 0.0;                  // suma de residuo^2
    double suma_b       = 0.0;                  // suma de b^2
    double suma_error   = 0.0;                  // suma de (x - 1)^2
    for (long fila = 0; fila < N; fila++) {
        double residuo = -b[fila];                          // (fila de A) . x - b
        for (long col = 0; col < N; col++) {
            residuo = residuo + A_original[fila*N + col] * x[col];
        }
        suma_residuo = suma_residuo + residuo * residuo;
        suma_b       = suma_b + b[fila] * b[fila];
        suma_error   = suma_error + (x[fila] - 1.0) * (x[fila] - 1.0);
    }
    *residuo_abs = sqrt(suma_residuo);
    *residuo_rel = sqrt(suma_residuo) / sqrt(suma_b);
    *error_x     = sqrt(suma_error) / sqrt(N);
}


int main(int argc, char *argv[]) {

    double t_inicio = tiempo_actual();

    if (argc != 2) {
        printf("Uso: ./lu_serial sistema_N.bin\n");
        return 1;
    }

    // ================= LECTURA =================
    FILE *archivo = fopen(argv[1], "rb");
    if (archivo == NULL) {
        printf("Error: no se pudo abrir %s\n", argv[1]);
        return 1;
    }

    // N es long: N*N se pasa del limite de un int desde N = 46341
    long N;
    long leidos_N = fread(&N, sizeof(long), 1, archivo);
    if (leidos_N != 1 || N <= 0) {
        printf("Error: %s no tiene un N valido\n", argv[1]);
        return 1;
    }

    double *A          = malloc(N * N * sizeof(double));   // se transforma en L y U
    double *A_original = malloc(N * N * sizeof(double));   // copia sin tocar, para ||Ax - b||
    double *b          = malloc(N * sizeof(double));
    double *x          = malloc(N * sizeof(double));
    long *pivote_del_paso = malloc(N * sizeof(long));
    if (A == NULL || A_original == NULL || b == NULL || x == NULL || pivote_del_paso == NULL) {
        printf("Error: no hay memoria suficiente\n");
        return 1;
    }

    long leidos_A = fread(A, sizeof(double), N * N, archivo);  // toda la matriz de una vez
    long leidos_b = fread(b, sizeof(double), N,     archivo);
    fclose(archivo);
    if (leidos_A != N * N || leidos_b != N) {
        printf("Error: %s esta incompleto\n", argv[1]);
        return 1;
    }

    double t_fin_lectura = tiempo_actual();

    // Copia de A (la factorizacion la destruye y los errores necesitan la original)
    for (long i = 0; i < N * N; i++) {
        A_original[i] = A[i];
    }

    // ================= FACTORIZAR =================
    double t_inicio_factorizar = tiempo_actual();   // marca propia: la copia no cuenta

    if (factorizar_lu(A, N, pivote_del_paso) != 0) {
        return 2;
    }

    double t_fin_factorizar = tiempo_actual();

    // ================= SUSTITUCION y ERRORES =================
    sustitucion(A, b, pivote_del_paso, x, N);

    double t_fin_sustitucion = tiempo_actual();

    double residuo_abs, residuo_rel, error_x;
    calcular_errores(A_original, b, x, N, &residuo_abs, &residuo_rel, &error_x);

    double t_fin = tiempo_actual();

    // ================= TIEMPOS =================
    double t_lectura     = t_fin_lectura     - t_inicio;
    double t_factorizar  = t_fin_factorizar  - t_inicio_factorizar;
    double t_sustitucion = t_fin_sustitucion - t_fin_factorizar;
    double t_total       = t_fin             - t_inicio;   // incluye la copia y los errores
    // Columnas de lu_mpi.c que en el serial valen esto:
    double t_comunicacion = 0.0;
    double t_calculo      = t_factorizar;
    double t_juntar       = 0.0;

    if (N <= 5) {                               // N chico: mostrar x para comprobar a mano
        for (long i = 0; i < N; i++) {
            printf("x[%ld] = %.6f\n", i, x[i]);
        }
    }

    printf("%ld,1,s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3e,%.3e,%.3e\n",
           N, t_total, t_lectura, t_factorizar, t_comunicacion, t_calculo, t_juntar, t_sustitucion,
           residuo_abs, residuo_rel, error_x);

    free(A);
    free(A_original);
    free(b);
    free(x);
    free(pivote_del_paso);
    return 0;
}

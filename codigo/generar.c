/*
 * generar.c - Proyecto 6: genera el sistema A x = b y lo guarda en UN archivo binario
 *
 *   A: matriz N x N con numeros al azar entre -1 y 1, en UN SOLO arreglo
 *      (el elemento de la fila i, columna j esta en A[i*N + j])
 *   b = A * x_real, con x_real = (1, 1, ..., 1)
 *      => b[fila] es la suma de los numeros de esa fila
 *
 * Archivo que se genera: sistema_N.bin, con este orden:
 *      N (1 long)  |  A (N*N doubles, fila detras de fila)  |  b (N doubles)
 *
 * Compilar:  gcc -O3 generar.c -o generar
 * Ejecutar:  ./generar N
 */

#include <stdio.h>      // printf, snprintf, fopen, fwrite, fclose
#include <stdlib.h>     // malloc, free, atol, rand, srand

int main(int argc, char *argv[])
{
    // ---------- Leer N ----------
    if (argc != 2)
    {
        printf("Uso: ./generar N\n");
        return 1;
    }

    long N = atol(argv[1]);     // long: asi fila*N no se pasa del limite con N grande

    if (N <= 0)                 // tambien atrapa texto: atol("hola") da 0
    {
        printf("Error: N debe ser mayor que 0\n");
        return 1;
    }

    // ---------- Reservar memoria ----------
    double *A = malloc(N * N * sizeof(double));     // la matriz: N*N numeros seguidos
    double *b = malloc(N * sizeof(double));         // el vector b: N numeros

    if (A == NULL || b == NULL)
    {
        printf("Error: no hay memoria suficiente\n");
        free(A);                // free(NULL) no hace nada, asi que es seguro
        free(b);
        return 1;
    }

    // ---------- Llenar A al azar y calcular b ----------
    srand(42);                  // semilla fija: con el mismo N, siempre la misma matriz

    for (long fila = 0; fila < N; fila++)
    {
        b[fila] = 0.0;

        for (long col = 0; col < N; col++)
        {
            A[fila*N + col] = 2.0 * rand() / RAND_MAX - 1.0;   // numero entre -1 y 1
            b[fila] += A[fila*N + col];                       // b = A*(1,...,1) = suma de la fila
        }
    }

    // ---------- Crear el archivo ----------
    char nombre_archivo[100];
    snprintf(nombre_archivo, sizeof(nombre_archivo), "sistema_%ld.bin", N);   // ej: sistema_1000.bin

    FILE *archivo = fopen(nombre_archivo, "wb");    // "wb" = escribir en binario

    if (archivo == NULL)
    {
        printf("Error: no se pudo crear %s\n", nombre_archivo);
        free(A);
        free(b);
        return 1;
    }

    // ---------- Guardar [N][A][b] ----------
    fwrite(&N, sizeof(long),   1,     archivo);     // 1) N  (con & porque es una variable suelta)
    fwrite(A,  sizeof(double), N * N, archivo);     // 2) toda la matriz, fila detras de fila
    fwrite(b,  sizeof(double), N,     archivo);     // 3) el vector b

    fclose(archivo);

    // ---------- Mostrar (solo si N es chico, para comprobar a mano) ----------
    if (N <= 5)
    {
        for (long fila = 0; fila < N; fila++)
        {
            for (long col = 0; col < N; col++){
                printf("%9.4f ", A[fila*N + col]); 
            }

            printf(" | b = %9.4f\n", b[fila]);
        }
    }

    printf("Guardado %s (N = %ld)\n", nombre_archivo, N);

    free(A);
    free(b);

    return 0;
}

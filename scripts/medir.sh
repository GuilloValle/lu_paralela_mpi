#!/bin/bash
# =====================================================================
# medir.sh - Proyecto 6: corre TODAS las mediciones y guarda los resultados en un CSV
#
# Que hace:
#   Para cada N de LISTA_N:
#     1) si no existe sistema_N.bin, lo genera con ./generar
#     2) corre el serial REPETICIONES veces          (./lu_serial, normal, sin mpirun)
#     3) para cada p de LISTA_P, y para cada reparto (c = ciclico, b = bloques),
#        corre el MPI REPETICIONES veces             (mpirun -n p --bind-to core ./lu_mpi)
#   Cada corrida imprime UNA linea CSV en pantalla; el script la agrega al archivo con >>
#
# Antes de usarlo:
#   - Compilar generar, lu_serial y lu_mpi (con -O3) en la misma carpeta que este script
#   - Darle permiso de ejecucion (una sola vez):   chmod +x medir.sh
#   - Notebook: enchufada, modo rendimiento, y no usarla mientras mide
#
# Uso:   ./medir.sh resultados_pc.csv
# =====================================================================


# ===================== CONFIGURACION (cambiar segun la maquina) =====================
LISTA_N="500 1000 2000 4000"   # PC: 500 1000 2000 4000  (8000 aparte)   | cluster: 500 1000 2000 4000 8000
LISTA_P="1 2 4 6 8 10"          # PC: 1 2 4 6 8 10                         | cluster: 1 2 4 8 16 32 64
REPETICIONES=10
# NUNCA poner en LISTA_P mas procesos que nucleos FISICOS (PC: 10, cluster: 96)


# ---------- Nombre del archivo de resultados (se pasa al ejecutar) ----------
ARCHIVO_CSV=$1
if [ -z "$ARCHIVO_CSV" ]; then          # -z = "esta vacio": no pasaron el nombre
    echo "Uso: ./medir.sh nombre_resultados.csv"
    exit 1
fi

# ---------- Encabezado del CSV: solo si el archivo todavia no existe ----------
# (asi, si corro el script dos veces con el mismo archivo, por ej. N=8000 aparte,
#  las lineas nuevas se agregan abajo y el encabezado no se repite)
if [ ! -f "$ARCHIVO_CSV" ]; then        # ! -f = "no existe ese archivo"
    echo "N,p,reparto,t_total,t_lectura,t_factorizar,t_comunicacion,t_calculo,t_juntar,t_sustitucion,residuo_abs,residuo_rel,error_x" > "$ARCHIVO_CSV"
fi

# ---------- Registro: hora y carga de la maquina (uptime) ----------
# Si despues un tiempo sale raro, aca se ve si la maquina estaba ocupada.
echo "=== Inicio: $(date) | carga: $(uptime)" >> registro_mediciones.txt


# ===================== MEDICIONES =====================
for N in $LISTA_N; do

    # ---------- 1) Generar la matriz si no existe ----------
    if [ ! -f "sistema_$N.bin" ]; then
        ./generar $N
    fi

    echo "--- N=$N empieza: $(date) | carga: $(uptime)" >> registro_mediciones.txt

    # ---------- 2) Serial: REPETICIONES veces ----------
    for rep in $(seq 1 $REPETICIONES); do          # seq 1 10 = 1 2 3 ... 10
        echo "N=$N  serial  repeticion $rep de $REPETICIONES"
        ./lu_serial "sistema_$N.bin" >> "$ARCHIVO_CSV"
    done

    # ---------- 3) MPI: cada p, cada reparto, REPETICIONES veces (seguidas) ----------
    for P in $LISTA_P; do
        for REPARTO in c b; do
            for rep in $(seq 1 $REPETICIONES); do
                echo "N=$N  p=$P  reparto=$REPARTO  repeticion $rep de $REPETICIONES"
                mpirun -n $P --bind-to core ./lu_mpi "sistema_$N.bin" $REPARTO >> "$ARCHIVO_CSV"
            done
        done
    done

done

echo "=== Fin: $(date)" >> registro_mediciones.txt
echo "Listo. Resultados en $ARCHIVO_CSV"

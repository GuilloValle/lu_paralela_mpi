#!/bin/bash
# =====================================================================
# medir_cluster.sh - Proyecto 6: mediciones en el CLUSTER (misma logica que medir.sh)
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
#   - Darle permiso de ejecucion (una sola vez):   chmod +x medir_cluster.sh
#   - Mirar uptime antes: que nadie mas este usando fuerte el cluster
#
# Uso:   nohup ./medir_cluster.sh resultados_cluster.csv > progreso.txt 2>&1 &
# =====================================================================


# ===================== CONFIGURACION (cambiar segun la maquina) =====================
LISTA_N="500 1000 2000 4000"         # tanda 1: 500 1000 2000 4000   |   tanda 2: 8000
LISTA_P="1 2 4 8 16 32 64 96"     # cluster (cuota de 96 nucleos)
REPETICIONES=10
# NUNCA poner en LISTA_P mas de 96 (la cuota del cluster)


# ---------- Nombre del archivo de resultados (se pasa al ejecutar) ----------
ARCHIVO_CSV=$1
if [ -z "$ARCHIVO_CSV" ]; then          # -z = "esta vacio": no pasaron el nombre
    echo "Uso: ./medir_cluster.sh nombre_resultados.csv"
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
# Estado de la cuota de CPU al empezar: nr_throttled = cuantas veces congelaron nuestros procesos
cat /sys/fs/cgroup/cpu.stat >> registro_mediciones.txt


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
# Estado de la cuota al terminar: si nr_throttled aumento respecto del inicio, hubo congelamientos
cat /sys/fs/cgroup/cpu.stat >> registro_mediciones.txt
echo "Listo. Resultados en $ARCHIVO_CSV"

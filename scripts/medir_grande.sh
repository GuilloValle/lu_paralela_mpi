#!/bin/bash
# =====================================================================
# medir_grande.sh - Proyecto 6: experimento con N GRANDES en el cluster
#
# Que hace:
#   0) ESPERA a que termine medir_cluster.sh (la tanda de N=8000), asi no se pisan
#   Despues, para cada N de LISTA_N (de menor a mayor):
#     1) si no existe sistema_N.bin, lo genera con ./generar
#     2) si N <= N_MAX_SERIAL: corre el serial UNA sola vez (con N grande tarda mucho)
#     3) para cada p de LISTA_P: corre el MPI REPETICIONES veces (solo reparto ciclico)
#   Cada corrida agrega una linea al CSV (igual que medir.sh)
#
# Por que de menor a mayor: si algo se corta (por ej. en N=30000),
# los N mas chicos ya quedaron guardados.
#
# Antes de usarlo:
#   - chmod +x medir_grande.sh   (una sola vez)
#
# Uso:   nohup ./medir_grande.sh resultados_cluster_grande.csv > progreso_grande.txt 2>&1 &
# =====================================================================


# ===================== CONFIGURACION =====================
LISTA_N="12000 16000 20000 30000"
LISTA_P="8 16 32 64 96"
REPETICIONES=2          # repeticiones del MPI (con N grande los tiempos varian menos del 1%)
N_MAX_SERIAL=20000      # el serial se corre UNA vez solo si N <= este valor (N=30000 tardaria ~1.5 h)


# ---------- Nombre del archivo de resultados (se pasa al ejecutar) ----------
ARCHIVO_CSV=$1
if [ -z "$ARCHIVO_CSV" ]; then          # -z = "esta vacio": no pasaron el nombre
    echo "Uso: ./medir_grande.sh nombre_resultados.csv"
    exit 1
fi

# ---------- Encabezado del CSV: solo si el archivo todavia no existe ----------
if [ ! -f "$ARCHIVO_CSV" ]; then
    echo "N,p,reparto,t_total,t_lectura,t_factorizar,t_comunicacion,t_calculo,t_juntar,t_sustitucion,residuo_abs,residuo_rel,error_x" > "$ARCHIVO_CSV"
fi


# ===================== 0) ESPERAR A QUE TERMINE LA TANDA DE N=8000 =====================
# pgrep -f busca un programa que este corriendo con ese nombre.
# Mientras lo encuentre, espera 60 segundos y vuelve a mirar.
while pgrep -f medir_cluster.sh > /dev/null; do
    echo "Esperando a que termine medir_cluster.sh ... $(date)"
    sleep 60
done


# ---------- Registro: hora, carga y cuota al empezar ----------
echo "=== GRANDES - Inicio: $(date) | carga: $(uptime)" >> registro_mediciones.txt
cat /sys/fs/cgroup/cpu.stat >> registro_mediciones.txt


# ===================== MEDICIONES =====================
for N in $LISTA_N; do

    # ---------- 1) Generar la matriz si no existe ----------
    if [ ! -f "sistema_$N.bin" ]; then
        echo "Generando sistema_$N.bin ... $(date)"
        ./generar $N
    fi

    echo "--- N=$N empieza: $(date) | carga: $(uptime)" >> registro_mediciones.txt

    # ---------- 2) Serial: UNA sola vez, y solo si N <= N_MAX_SERIAL ----------
    if [ $N -le $N_MAX_SERIAL ]; then           # -le = "menor o igual"
        echo "N=$N  serial (una vez)  $(date)"
        ./lu_serial "sistema_$N.bin" >> "$ARCHIVO_CSV"
    fi

    # ---------- 3) MPI: cada p, solo ciclico, REPETICIONES veces ----------
    for P in $LISTA_P; do
        for rep in $(seq 1 $REPETICIONES); do
            echo "N=$N  p=$P  reparto=c  repeticion $rep de $REPETICIONES  $(date)"
            mpirun -n $P --bind-to core ./lu_mpi "sistema_$N.bin" c >> "$ARCHIVO_CSV"
        done
    done

done

echo "=== GRANDES - Fin: $(date)" >> registro_mediciones.txt
cat /sys/fs/cgroup/cpu.stat >> registro_mediciones.txt
echo "Listo. Resultados en $ARCHIVO_CSV"

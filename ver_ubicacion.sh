#!/bin/bash
# =====================================================================
# ver_ubicacion.sh - Proyecto 6: muestra DONDE ubica MPI cada proceso
# y cuantos procesos comparten cada cache L3 (grupo de 4 nucleos).
#
# Uso:   ./ver_ubicacion.sh sistema_1000.bin > ubicacion_procesos.txt
#        (necesita lu_mpi compilado en la misma carpeta)
# =====================================================================

ARCHIVO=$1
if [ -z "$ARCHIVO" ]; then
    echo "Uso: ./ver_ubicacion.sh sistema_1000.bin"
    exit 1
fi

echo "=== Cache L3 del procesador ==="
echo "Tamano de la L3:                 $(cat /sys/devices/system/cpu/cpu0/cache/index3/size)"
echo "Nucleos que comparten la L3 del nucleo 0:  $(cat /sys/devices/system/cpu/cpu0/cache/index3/shared_cpu_list)"
echo "Nucleos que comparten la L3 del nucleo 64: $(cat /sys/devices/system/cpu/cpu64/cache/index3/shared_cpu_list)"
echo

for P in 2 4 8 16 32; do
    echo "=== p = $P   (mpirun -n $P --bind-to core) ==="
    # Corre el programa, se queda con "rank R ... socket S[core C" y arma una tabla
    mpirun -n $P --bind-to core --report-bindings ./lu_mpi "$ARCHIVO" c 2>&1 \
      | grep -o "rank [0-9]* bound to socket [01]\[core [0-9]*" \
      | sed 's/rank \([0-9]*\) bound to socket \([01]\)\[core \([0-9]*\)/\1 \2 \3/' \
      | sort -n \
      | awk '{
            grupo = int($3 / 4) * 4;                       # primer nucleo del grupo de 4
            printf "  proceso %2d -> socket %d, nucleo %3d, L3 de los nucleos %d-%d\n", $1, $2, $3, grupo, grupo + 3;
            cuenta[grupo]++;
        }
        END {
            printf "  Procesos por cada L3 usada:";
            for (g in cuenta) {
                printf "  [%d-%d]: %d", g, g + 3, cuenta[g];
            }
            printf "\n";
        }'
    echo
done

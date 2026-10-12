# =====================================================================
# graficos.py - Proyecto 6: graficos de las mediciones (LU paralela con MPI)
#
# Lee los archivos CSV de resultados (tienen que estar en la misma carpeta):
#   resultados_pc.csv              -> mi notebook (N = 500..4000, p = 1..10)
#   resultados_cluster.csv         -> cluster     (N = 500..8000, p = 1..96)
#   resultados_cluster_grande.csv  -> cluster     (N = 12000..30000, p = 8..96)
#   experimento_ubicacion.csv      -> experimento de la cache L3 (solo para N4)
#
# Cada caso (N, p, reparto) se corrio varias veces: siempre usamos la MEDIANA.
#
# Que hace: genera los 8 graficos del informe y los guarda como .png en la
# carpeta "figuras" (la crea si no existe). Los nombres empiezan con un numero
# (01_, 02_, ...) para que queden ordenados.
# Si MOSTRAR_VENTANAS = True, ademas abre una ventana por cada grafico al final.
#
# Ejecutar:   python3 graficos.py
# Necesita:   numpy y matplotlib  (si faltan:  pip install numpy matplotlib)
# =====================================================================

import csv
import os
import numpy as np
import matplotlib.pyplot as plt

MOSTRAR_VENTANAS = False        # True = abre las 8 ventanas al final; False = solo guarda los .png

# Trabajar siempre en la carpeta donde esta este archivo (ahi estan los CSV),
# asi funciona igual si se ejecuta desde otra carpeta
os.chdir(os.path.dirname(os.path.abspath(__file__)))

CARPETA = "figuras"             # carpeta donde se guardan las imagenes
os.makedirs(CARPETA, exist_ok=True)   # crea la carpeta (si ya existe, no hace nada)


# Guarda el grafico actual como imagen .png dentro de la carpeta
def guardar(nombre_archivo):
    plt.savefig(os.path.join(CARPETA, nombre_archivo), dpi=120)
    print("Guardado:", os.path.join(CARPETA, nombre_archivo))


# =====================================================================
# PARTE 1: LEER LOS CSV
# =====================================================================

# Lee un CSV y devuelve una lista de filas. Cada fila es un "diccionario":
# se accede a cada columna por su nombre, por ejemplo fila["t_factorizar"].
# A cada fila le agregamos de que maquina viene ("PC" o "CLUSTER").
def leer_csv(nombre_archivo, maquina):
    filas = []
    with open(nombre_archivo) as archivo:
        lector = csv.DictReader(archivo)     # usa la primera linea (el encabezado) como nombres
        for fila in lector:
            fila["maquina"] = maquina
            filas.append(fila)
    return filas


# Todas las mediciones juntas en una sola lista
datos = []
datos = datos + leer_csv("resultados_pc.csv", "PC")
datos = datos + leer_csv("resultados_cluster.csv", "CLUSTER")
datos = datos + leer_csv("resultados_cluster_grande.csv", "CLUSTER")


# =====================================================================
# PARTE 2: FUNCIONES PARA SACAR NUMEROS DE LOS DATOS
# =====================================================================

# Mediana de una columna para un caso: junta todas las repeticiones de
# (maquina, N, p, reparto) y devuelve la del medio.
# reparto: "s" = serial, "c" = ciclico, "b" = bloques.
def mediana(maquina, N, p, reparto, columna):
    valores = []
    for fila in datos:
        if fila["maquina"] == maquina and int(fila["N"]) == N and int(fila["p"]) == p and fila["reparto"] == reparto:
            valores.append(float(fila[columna]))
    return np.median(valores)


# Speedup = tiempo serial / tiempo paralelo (reparto ciclico).
# columna = "t_factorizar" (solo la factorizacion) o "t_total" (programa completo).
def speedup(maquina, N, p, columna):
    t_serial = mediana(maquina, N, 1, "s", columna)
    t_paralelo = mediana(maquina, N, p, "c", columna)
    return t_serial / t_paralelo


# =====================================================================
# PARTE 3: QUE N Y QUE p SE MIDIERON EN CADA MAQUINA
# =====================================================================
N_PC = [500, 1000, 2000, 4000]
P_PC = [1, 2, 4, 6, 8, 10]

N_CLUSTER = [500, 1000, 2000, 4000, 8000]          # con serial y p = 1..96
P_CLUSTER = [1, 2, 4, 8, 16, 32, 64, 96]

N_GRANDES = [12000, 16000, 20000]                  # con serial (1 vez) y p = 8..96
P_GRANDES = [8, 16, 32, 64, 96]

N_CON_SERIAL = N_CLUSTER + N_GRANDES               # 500 .. 20000 (sirven para speedup)
N_TODOS = N_CLUSTER + N_GRANDES + [30000]          # 500 .. 30000 (30000 no tiene serial)

EPSILON = 2.2e-16       # epsilon de maquina de un double: el error relativo mas chico posible

# Posiciones para poner los p del cluster (1, 2, 4, 8, ..., 96) a DISTANCIAS IGUALES en el eje x:
# si no, los p chicos quedan todos amontonados a la izquierda.
posiciones_p = np.arange(len(P_CLUSTER))     # 0, 1, 2, ... una posicion por cada p
etiquetas_p = []
for p in P_CLUSTER:
    etiquetas_p.append(str(p))

# Textos para el eje x: "500", "1000", ... (para escribir los N tal cual, sin potencias de 10)
etiquetas_N = []
for N in N_TODOS:
    etiquetas_N.append(str(N))

# Marcas del eje de TIEMPO escritas en unidades faciles de leer (en vez de 10^-2, 10^3, ...).
# El eje sigue siendo logaritmico (los tiempos van de 0,005 s a 30 min), solo cambian los textos.
MARCAS_TIEMPO = [0.01, 0.1, 1, 10, 60, 600, 3600]
TEXTOS_TIEMPO = ["0,01 s", "0,1 s", "1 s", "10 s", "1 min", "10 min", "1 h"]


# Escribe un tiempo en segundos con la unidad mas comoda: ms, s o min.
# Ej: 0.0143 -> "14 ms",  12.84 -> "12,8 s",  769.3 -> "12,8 min"
def texto_tiempo(segundos):
    if segundos < 1:
        texto = f"{segundos * 1000:.0f} ms"
    elif segundos < 60:
        texto = f"{segundos:.1f} s"
    else:
        texto = f"{segundos / 60:.1f} min"
    return texto.replace(".", ",")      # coma decimal


# =====================================================================
# G1: TIEMPO DE FACTORIZACION vs N  (escala log en los dos ejes)
# Muestra como crece el tiempo de computo al aumentar N.
# En escala log-log, algo que crece como N^3 se ve como una RECTA.
# =====================================================================
t_serial_pc = []
for N in N_PC:
    t_serial_pc.append(mediana("PC", N, 1, "s", "t_factorizar"))

t_serial_cluster = []
for N in N_CON_SERIAL:
    t_serial_cluster.append(mediana("CLUSTER", N, 1, "s", "t_factorizar"))

# Los p que estan medidos para TODOS los N (de 500 a 30000): 8, 16, 32, 64 y 96.
# (p = 1, 2 y 4 solo se midieron hasta N=8000, por eso no van aca)
P_TODOS_LOS_N = [8, 16, 32, 64, 96]

# Recta de referencia "crece como N^3": la hacemos pasar por el serial del cluster en N=8000
t_8000 = mediana("CLUSTER", 8000, 1, "s", "t_factorizar")
referencia_N3 = []
for N in N_TODOS:
    referencia_N3.append(t_8000 * (N / 8000) ** 3)

# Dos paneles para que no se amontonen las lineas:
#   izquierda: los dos seriales (como crece el computo con N, comparado con N^3)
#   derecha:   el cluster, serial y algunos p (p=16 queda casi encima de p=8 y p=64 encima de p=96)
marcadores = ["o", "s", "^", "v", "D"]   # un simbolo distinto para cada linea
plt.figure(figsize=(13, 5))

plt.subplot(1, 2, 1)                     # panel izquierdo
plt.loglog(N_PC, t_serial_pc, "o-", markersize=7, label="Serial PC")
plt.loglog(N_CON_SERIAL, t_serial_cluster, "s-", markersize=7, label="Serial cluster")
plt.loglog(N_TODOS, referencia_N3, "k--", label="Referencia: crece como N^3")
plt.xlabel("N (tamano de la matriz)")
plt.ylabel("Tiempo de factorizacion")
plt.title("Serial: PC y cluster")
plt.ylim(0.006, 6000)                    # un poco de lugar abajo para que entre el texto "13 ms"
# En escala log, matplotlib pone marcas en 10^3 = 1000 y 10^4 = 10000.
# Con estas dos lineas ponemos las marcas justo en los N medidos, escritos normal.
plt.xticks(N_TODOS, etiquetas_N, rotation=45)
plt.yticks(MARCAS_TIEMPO, TEXTOS_TIEMPO)
plt.minorticks_off()                     # saca las rayitas chicas entre marcas
plt.grid()
plt.legend()
# Escribir el valor al lado de cada punto: cluster ARRIBA del punto, PC ABAJO
# (los dos estan casi en el mismo lugar, asi no se pisan los textos)
for i in range(len(N_CON_SERIAL)):
    plt.annotate(texto_tiempo(t_serial_cluster[i]), (N_CON_SERIAL[i], t_serial_cluster[i]),
                 textcoords="offset points", xytext=(-8, 9), ha="right", fontsize=8, color="tab:orange")
for i in range(len(N_PC)):
    plt.annotate(texto_tiempo(t_serial_pc[i]), (N_PC[i], t_serial_pc[i]),
                 textcoords="offset points", xytext=(8, -12), ha="left", fontsize=8, color="tab:blue")

plt.subplot(1, 2, 2)                     # panel derecho
plt.loglog(N_CON_SERIAL, t_serial_cluster, "k-", linewidth=2.5, label="Serial cluster")
numero_linea = 0
for p in [8, 32, 96]:
    t_p = []
    for N in N_TODOS:
        t_p.append(mediana("CLUSTER", N, p, "c", "t_factorizar"))
    plt.loglog(N_TODOS, t_p, marcadores[numero_linea] + "-", markersize=7, label="Cluster p=" + str(p))
    numero_linea = numero_linea + 1
plt.xlabel("N (tamano de la matriz)")
plt.ylabel("Tiempo de factorizacion")
plt.title("Cluster: serial y paralelo")
plt.xticks(N_TODOS, etiquetas_N, rotation=45)
plt.yticks(MARCAS_TIEMPO, TEXTOS_TIEMPO)
plt.minorticks_off()
plt.grid()
plt.legend()

plt.suptitle("G1: tiempo de factorizacion vs N")
plt.tight_layout()
guardar("01_G1_tiempo_vs_N.png")

# Tabla en pantalla con los tiempos del panel derecho (en el grafico no entran los 36 numeros)
print("\nG1 - tiempo de factorizacion en el cluster (mediana):")
print(f"{'N':>6} {'serial':>10} {'p=8':>10} {'p=32':>10} {'p=96':>10}")
for N in N_TODOS:
    linea = f"{N:>6}"
    if N in N_CON_SERIAL:
        linea = linea + f" {texto_tiempo(mediana('CLUSTER', N, 1, 's', 't_factorizar')):>10}"
    else:
        linea = linea + f" {'-':>10}"
    for p in [8, 32, 96]:
        linea = linea + f" {texto_tiempo(mediana('CLUSTER', N, p, 'c', 't_factorizar')):>10}"
    print(linea)
print()


# =====================================================================
# G2: CALCULO vs COMUNICACION al aumentar N, cluster con p = 16, 32 y 96
# Un panel por cada p (los mismos p que E3). En cada panel, la linea que esta
# mas arriba es la que manda en el tiempo. Los tiempos estan en segundos.
# =====================================================================
plt.figure(figsize=(15, 4.8))
numero_panel = 1
for p in [16, 32, 96]:
    calculo = []
    comunicacion = []
    for N in N_TODOS:
        calculo.append(mediana("CLUSTER", N, p, "c", "t_calculo"))
        comunicacion.append(mediana("CLUSTER", N, p, "c", "t_comunicacion"))
    plt.subplot(1, 3, numero_panel)      # 1 fila, 3 columnas, panel 1, 2 o 3
    plt.loglog(N_TODOS, calculo, "o-", label="Calculo")
    plt.loglog(N_TODOS, comunicacion, "s-", label="Comunicacion (incluye espera)")
    plt.xlabel("N (tamano de la matriz)")
    plt.ylabel("Tiempo [s]")
    # En escala log, matplotlib pone marcas en 10^3 = 1000 y 10^4 = 10000.
    # Con estas dos lineas ponemos las marcas justo en los N medidos, escritos normal.
    plt.xticks(N_TODOS, etiquetas_N, rotation=45)
    plt.minorticks_off()                 # saca las rayitas chicas entre marcas
    plt.ylim(1e-4, 3e3)                  # mismo eje vertical en los 3 paneles, para poder compararlos
    plt.title("p = " + str(p))
    plt.grid()
    if numero_panel == 1:
        plt.legend(loc="upper left")
    numero_panel = numero_panel + 1
plt.suptitle("G2: calculo vs comunicacion al aumentar N (cluster)")
plt.tight_layout()
guardar("02_G2_calculo_vs_comunicacion.png")


# =====================================================================
# G3b: SPEEDUP vs p en el cluster, una linea por N, y la linea ideal
# Para N grandes no hay p < 8. El eje vertical se corta en 40 para que se vean
# las curvas (la linea ideal sigue hasta 96, se sale del grafico).
# El cuadro de referencias (leyenda) va AFUERA, a la derecha, para no tapar las curvas.
# =====================================================================
plt.figure(figsize=(9, 5))
for N in N_CLUSTER:
    s = []
    for p in P_CLUSTER:
        s.append(speedup("CLUSTER", N, p, "t_factorizar"))
    plt.plot(P_CLUSTER, s, "o-", label="N=" + str(N))
for N in N_GRANDES:
    s = []
    for p in P_GRANDES:
        s.append(speedup("CLUSTER", N, p, "t_factorizar"))
    plt.plot(P_GRANDES, s, "s--", label="N=" + str(N))
plt.plot(P_CLUSTER, P_CLUSTER, "k--", label="Ideal (speedup = p)")
plt.ylim(0, 40)
# Eje x en escala log base 2: asi 1, 2, 4, 8, ..., 64 quedan a la MISMA distancia
# (cada marca es el doble de la anterior) y los p chicos no se amontonan a la izquierda.
# Las marcas se ponen justo en los p medidos.
plt.xscale("log", base=2)
plt.xticks(P_CLUSTER, etiquetas_p)
plt.minorticks_off()
plt.xlabel("p (cantidad de procesos, escala log: cada marca es el doble)")
plt.ylabel("Speedup de la factorizacion")
plt.title("G3b: speedup vs p, cluster")
plt.grid()
plt.legend(loc="upper left", bbox_to_anchor=(1.02, 1))   # (1.02, 1) = justo afuera del borde derecho, arriba
plt.tight_layout()                                         # achica el grafico para que entre la leyenda
guardar("03_G3b_speedup_vs_p_cluster.png")


# =====================================================================
# N4: EXPERIMENTO DE UBICACION (cache L3). N4 sale de OTRO archivo: experimento_ubicacion.csv
# Mismo N, mismos 16 procesos, mismo codigo. Solo cambia DONDE se ubican los procesos:
#   4_por_L3 -> mpirun -n 16 --map-by ppr:4:l3cache --bind-to core ...  (4 comparten cada L3 de 16 MB: 4 MB c/u)
#   1_por_L3 -> mpirun -n 16 --map-by ppr:1:l3cache --bind-to core ...  (cada uno tiene la L3 entera: 16 MB)
# N=4000: cada proceso tiene 8 MB  -> con 1 por L3 ENTRA en la cache
# N=8000: cada proceso tiene 32 MB -> NO entra en ningun caso (sirve de control)
# =====================================================================
if os.path.exists("experimento_ubicacion.csv"):
    experimento = []
    with open("experimento_ubicacion.csv") as archivo:
        for fila in csv.DictReader(archivo):
            experimento.append(fila)

    # Mediana de t_factorizar para una ubicacion y un N
    def mediana_ubicacion(ubicacion, N):
        valores = []
        for fila in experimento:
            if fila["ubicacion"] == ubicacion and int(fila["N"]) == N:
                valores.append(float(fila["t_factorizar"]))
        return np.median(valores)

    plt.figure(figsize=(11, 5))
    numero_panel = 1
    for N in [4000, 8000]:
        t_4_por_L3 = mediana_ubicacion("4_por_L3", N)
        t_1_por_L3 = mediana_ubicacion("1_por_L3", N)
        mb_por_proceso = N * N * 8 / 16 / 1e6

        plt.subplot(1, 2, numero_panel)
        etiquetas = ["4 procesos por L3\n(4 MB de cache c/u)", "1 proceso por L3\n(16 MB de cache c/u)"]
        tiempos = [t_4_por_L3, t_1_por_L3]
        plt.bar([0, 1], tiempos, color=["tab:blue", "tab:orange"])
        plt.xticks([0, 1], etiquetas)
        for i in range(2):
            plt.text(i, tiempos[i], texto_tiempo(tiempos[i]), ha="center", va="bottom", fontsize=10)
        plt.ylim(0, t_4_por_L3 * 1.25)
        plt.ylabel("Tiempo de factorizacion")

        if mb_por_proceso <= 16:
            entra = "con 1 por L3 SI entra en la cache"
        else:
            entra = "NO entra en la cache en ningun caso"
        plt.title(f"N = {N}: {mb_por_proceso:.0f} MB por proceso\n({entra})")
        # Cuantas veces mas rapido, escrito en el medio del panel
        plt.text(0.5, t_4_por_L3 * 1.1, f"{t_4_por_L3 / t_1_por_L3:.1f} veces mas rapido".replace(".", ","),
                 ha="center", fontsize=11, fontweight="bold")
        plt.grid(axis="y")
        numero_panel = numero_panel + 1

    plt.suptitle("N4: mismos 16 procesos, solo cambia cuantos comparten cada cache L3 (16 MB)")
    plt.tight_layout()
    guardar("04_N4_experimento_cache.png")
else:
    print("No esta experimento_ubicacion.csv: no se hace el grafico N4")


# =====================================================================
# E5: TIEMPO DE EJECUCION (t_total) PARA CADA CANTIDAD DE PROCESOS, un panel por N
# Cada barra es la mediana de t_total con ese p (reparto ciclico).
# En naranja, la cantidad de procesos mas rapida. La linea punteada es el serial.
# (t_total no incluye arrancar MPI: ~1 s por corrida en el cluster, ~0.4 s en la PC)
# =====================================================================

# Dibuja un panel de barras para una maquina y un N.
def panel_barras(maquina, N, lista_p):
    tiempos = []
    etiquetas = []
    for p in lista_p:
        tiempos.append(mediana(maquina, N, p, "c", "t_total"))
        etiquetas.append(str(p))

    # el p mas rapido = el que tiene el tiempo mas chico
    indice_mas_rapido = int(np.argmin(tiempos))
    colores = []
    for i in range(len(tiempos)):
        if i == indice_mas_rapido:
            colores.append("tab:orange")
        else:
            colores.append("tab:blue")

    posiciones = np.arange(len(lista_p))
    plt.bar(posiciones, tiempos, color=colores)
    plt.xticks(posiciones, etiquetas)

    # Escribir el tiempo ARRIBA de cada barra (en ms, s o min segun el tamano)
    for i in range(len(tiempos)):
        plt.text(posiciones[i], tiempos[i], texto_tiempo(tiempos[i]),
                 ha="center", va="bottom", fontsize=8)

    # linea punteada del serial (si se midio para este N)
    t_serial = 0
    if N in N_PC or N in N_CON_SERIAL:
        t_serial = mediana(maquina, N, 1, "s", "t_total")
        plt.axhline(t_serial, color="k", linestyle=":", label="serial: " + texto_tiempo(t_serial))
        plt.legend()

    # Un 15 % de lugar arriba para que entren los textos de las barras mas altas
    plt.ylim(0, max(max(tiempos), t_serial) * 1.15)

    plt.xlabel("Procesos")
    plt.ylabel("Tiempo total [s]")
    plt.title("N = " + str(N) + "  (mas rapido: " + str(lista_p[indice_mas_rapido]) + " procesos)")
    plt.grid(axis="y")


# --- Cluster: 9 paneles (3 filas x 3 columnas) ---
plt.figure(figsize=(16, 13))
numero_panel = 1
for N in N_TODOS:
    plt.subplot(3, 3, numero_panel)
    if N in N_CLUSTER:
        panel_barras("CLUSTER", N, P_CLUSTER)      # N <= 8000: p = 1 .. 96
    else:
        panel_barras("CLUSTER", N, P_GRANDES)      # N >= 12000: p = 8 .. 96
    numero_panel = numero_panel + 1
plt.suptitle("E5: tiempo de ejecucion en el cluster (en naranja, la cantidad de procesos mas rapida)")
plt.tight_layout()
guardar("05_E5_tiempo_por_p_cluster.png")

# --- PC: 4 paneles (2 filas x 2 columnas) ---
plt.figure(figsize=(12, 8))
numero_panel = 1
for N in N_PC:
    plt.subplot(2, 2, numero_panel)
    panel_barras("PC", N, P_PC)
    numero_panel = numero_panel + 1
plt.suptitle("E5: tiempo de ejecucion en la PC (en naranja, la cantidad de procesos mas rapida)")
plt.tight_layout()
guardar("06_E5_tiempo_por_p_PC.png")


# =====================================================================
# E4: CICLICO vs BLOQUES. Diferencia en %:
#   diferencia = (t_ciclico - t_bloques) / t_bloques * 100
#   0% = iguales; positivo = ciclico mas lento; negativo = ciclico mas rapido.
# El eje va de -5% a +5% para que se vean separadas las lineas
# (todas quedan dentro de +-2%: los dos repartos dan practicamente igual).
# Ademas se imprime la tabla en la terminal.
# =====================================================================
plt.figure()
print("E4: ciclico vs bloques (cluster, mediana de t_factorizar)")
print("    N    p   ciclico[s]   bloques[s]   diferencia[%]")
numero_linea = 0
for N in N_CLUSTER:
    diferencia = []
    for p in P_CLUSTER:
        t_ciclico = mediana("CLUSTER", N, p, "c", "t_factorizar")
        t_bloques = mediana("CLUSTER", N, p, "b", "t_factorizar")
        diferencia_porcentaje = (t_ciclico - t_bloques) / t_bloques * 100
        diferencia.append(diferencia_porcentaje)
        print(f"{N:5d} {p:4d} {t_ciclico:12.4f} {t_bloques:12.4f} {diferencia_porcentaje:12.2f}")
    plt.plot(posiciones_p, diferencia, marcadores[numero_linea] + "-", label="N=" + str(N))
    numero_linea = numero_linea + 1
plt.plot(posiciones_p, np.zeros(len(P_CLUSTER)), "k--", label="Iguales (0%)")
plt.xticks(posiciones_p, etiquetas_p)    # p a distancias iguales
plt.ylim(-5, 5)
plt.xlabel("p (cantidad de procesos)")
plt.ylabel("Diferencia ciclico vs bloques [%]")
plt.title("E4: ciclico vs bloques, cluster")
plt.grid()
plt.legend()
guardar("07_E4_ciclico_vs_bloques.png")


# =====================================================================
# G6: PRECISION. Residuo absoluto ||Ax-b|| y relativo ||Ax-b||/||b|| vs N
# El residuo es el mismo para todos los p y repartos (y en las 2 maquinas),
# asi que alcanza con tomar una sola corrida de cada N.
# (Sin la recta de referencia N*epsilon: la comparacion con N*epsilon se explica de palabra.)
# =====================================================================
residuo_abs = []
residuo_rel = []
for N in N_TODOS:
    for fila in datos:                     # busco la primera fila del cluster con ese N
        if fila["maquina"] == "CLUSTER" and int(fila["N"]) == N:
            residuo_abs.append(float(fila["residuo_abs"]))
            residuo_rel.append(float(fila["residuo_rel"]))
            break                          # ya encontre una: corto la busqueda

plt.figure()
plt.loglog(N_TODOS, residuo_abs, "o-", label="Residuo absoluto ||Ax-b||")
plt.loglog(N_TODOS, residuo_rel, "s-", label="Residuo relativo ||Ax-b|| / ||b||")
plt.xlabel("N (tamano de la matriz)")
plt.ylabel("Residuo")
# En escala log, matplotlib pone marcas en 10^3 = 1000 y 10^4 = 10000.
# Con estas dos lineas ponemos las marcas justo en los N medidos, escritos normal.
plt.xticks(N_TODOS, etiquetas_N, rotation=45)
plt.minorticks_off()                     # saca las rayitas chicas entre marcas
plt.title("G6: precision vs N")
plt.grid()
plt.legend()
plt.tight_layout()                       # para que no se corte el nombre del eje x
guardar("08_G6_precision_vs_N.png")


# =====================================================================
# Mostrar los graficos en ventanas (solo si MOSTRAR_VENTANAS = True)
# =====================================================================
print("Listo: todos los graficos estan en la carpeta", CARPETA)
if MOSTRAR_VENTANAS:
    plt.show()

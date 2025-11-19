# monitor_completo.py  ← GUARDA ASÍ
from flask import Flask, jsonify
import threading
import time
import os
import re

app = Flask(__name__)
ARCHIVO = r"C:\Users\aledi\Desktop\live_data.txt"

# Variables globales
tiempos = []
valores_adc = []
estado_puerta = "DESCONOCIDO"   # será ABIERTA o CERRADA
lock = threading.Lock()

def lector():
    global tiempos, valores_adc, estado_puerta
    posicion = 0

    while True:
        if not os.path.exists(ARCHIVO):
            time.sleep(1)
            continue

        try:
            with open(ARCHIVO, "r", encoding="utf-8", errors="ignore") as f:
                f.seek(0, 2)
                tamano = f.tell()
                if tamano < posicion:
                    posicion = 0
                f.seek(posicion)
                lineas = f.readlines()
                posicion = f.tell()

                for linea in lineas:
                    linea = linea.strip()

                    # 1. Extraer valor ADC
                    match_adc = re.search(r"ADC:\s*(\d+)", linea)
                    if match_adc:
                        valor = int(match_adc.group(1))
                        with lock:
                            tiempos.append(len(tiempos)+1)
                            valores_adc.append(valor)
                            if len(valores_adc) > 200:
                                valores_adc.pop(0)
                                tiempos.pop(0)

                    # 2. Detectar estado de la puerta
                    if "puerta esta abierta" in linea.lower():
                        estado_puerta = "ABIERTA"
                    elif "puerta esta cerrada" in linea.lower():
                        estado_puerta = "CERRADA"

        except:
            pass
        time.sleep(0.1)

@app.route("/")
def index():
    return f"""
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="utf-8">
        <title>Monitor Calidad Aire + Puerta</title>
        <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
        <style>
            body {{background:#000; color:#0f0; font-family:Arial; text-align:center; padding:20px;}}
            canvas {{background:#111; border:3px solid #0f0; border-radius:15px;}}
            .puerta {{font-size: 60px; font-weight: bold; margin:30px; padding:20px; border-radius:20px;}}
            .abierta {{background:#c00; color:white;}}
            .cerrada {{background:#0c0; color:black;}}
        </style>
    </head>
    <body>
        <h1>Tiva TM4C1294 – Monitorización en vivo</h1>

        <div class="puerta" id="estado_puerta">Cargando estado de la puerta...</div>

        <canvas id="chart" width="1100" height="450"></canvas>
        <h2 id="valor_actual">Buscando datos ADC...</h2>

        <script>
            const chart = new Chart(document.getElementById('chart'), {{
                type: 'line',
                data: {{labels: [], datasets: [{{
                    label: 'Valor ADC (calidad del aire)',
                    data: [],
                    borderColor: '#00ff88',
                    backgroundColor: 'rgba(0,255,136,0.2)',
                    fill: true,
                    tension: 0.4
                }}]}},
                options: {{animation: false, scales: {{y: {{min: 0, max: 4095}}}}}}
            }});

            setInterval(async () => {{
                const r = await fetch("/data");
                const d = await r.json();
                chart.data.labels = d.t;
                chart.data.datasets[0].data = d.v;
                chart.update();

                if (d.v.length > 0) {{
                    const ultimo = d.v[d.v.length-1];
                    const voltaje = (ultimo * 3.3 / 4095).toFixed(3);
                    document.getElementById("valor_actual").innerHTML = 
                        `<b>ADC:</b> ${{-ultimo}} → <b>Voltaje:</b> ${{voltaje}} V`;

                    // Actualizar estado de la puerta
                    const div = document.getElementById("estado_puerta");
                    div.textContent = "PUERTA " + d.puerta;
                    if (d.puerta === "ABIERTA") {{
                        div.className = "puerta abierta";
                    }} else {{
                        div.className = "puerta cerrada";
                    }}
                }}
            }}, 500);
        </script>
    </body>
    </html>
    """

@app.route("/data")
def data():
    with lock:
        return jsonify({
            "t": tiempos.copy(),
            "v": valores_adc.copy(),
            "puerta": estado_puerta
        })

# Arrancar lector
threading.Thread(target=lector, daemon=True).start()
time.sleep(1)

if __name__ == "__main__":
    print("¡TODO LISTO! Abre: http://10.100.46.214:5000")
    app.run(host="0.0.0.0", port=5000)
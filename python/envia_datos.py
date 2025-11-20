# todo_en_uno_FINAL.py  ← ESTE SÍ FUNCIONA CON TU LÍNEA EXACTA
from flask import Flask, jsonify           # Importa Flask para web y jsonify para enviar datos en JSON
import serial                              # Para comunicación con el puerto serial (Tiva)
import threading                           # Para ejecutar la lectura serial en un hilo aparte
import time                                # Para pausas breves
import re                                  # Para usar expresiones regulares y extraer ADC

app = Flask(__name__)                      # Crea la aplicación web Flask

PUERTO = "COM4"                            # Puerto donde está conectada la Tiva
ARCHIVO = r"C:\Users\aledi\Desktop\live_data.txt"   # Ruta del archivo donde se guardarán los datos

# Abre el puerto serial con baudrate 115200 y timeout 0.1
ser = serial.Serial(PUERTO, 115200, timeout=0.1)
print(f"Conectado a {PUERTO} – esperando datos de la Tiva...")

# Lista circular para guardar las últimas 200 muestras del ADC
datos_adc = []

# Variable global que almacena el estado actual del aire
estado_actual = "CARGANDO..."

# ====================== HILO LECTOR SERIAL ======================
def lector_tiva():
    global datos_adc, estado_actual
    while True:                            # Bucle infinito en el hilo
        if ser.in_waiting > 0:             # Si hay datos disponibles en el buffer serial
            try:
                # Leer una línea, decodificar UTF-8 y limpiar espacios
                linea = ser.readline().decode('utf-8', errors='ignore').strip()
                if not linea:
                    continue               # Si está vacía, se ignora

                print(linea)               # Mostrar la línea recibida en consola

                # Guardar línea completa en el archivo de log
                with open(ARCHIVO, "a", encoding="utf-8") as f:
                    f.write(linea + "\n")

                # Buscar número ADC usando expresión regular
                match = re.search(r"ADC:\s*(\d+)", linea)
                if match:
                    adc = int(match.group(1))   # Extrae el ADC como entero
                    datos_adc.append(adc)       # Lo agrega al buffer
                    if len(datos_adc) > 200:    # Si supera 200 valores
                        datos_adc.pop(0)        # Eliminar el más viejo (FIFO)

                # Detectar estado según palabras clave recibidas
                if "AIRE LIMPIO" in linea:
                    estado_actual = "AIRE LIMPIO"
                elif "GAS" in linea or "ALARMA" in linea:
                    estado_actual = "GAS DETECTADO"

            except:
                pass                   # Ignora cualquier error y continúa leyendo
        time.sleep(0.01)               # Pausa para no saturar CPU

# ====================== RUTA PRINCIPAL WEB ======================
@app.route("/")
def web():
    return '''   # <-- retorna HTML completo como texto
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset="utf-8">
        <title>Detector Gas - FUNCIONANDO</title>
        <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
        <style>
            body{background:#000;color:#0f0;font-family:Arial;text-align:center;padding:20px}
            canvas{background:#111;border:5px solid #0f0;border-radius:20px;margin:30px auto;max-width:95%}
            .estado{font-size:100px;padding:50px;border-radius:40px;margin:40px}
            .limpio{background:#0f0;color:black}
            .gas{background:#c00;color:white;animation:parp 1s infinite}
            @keyframes parp{50%{opacity:0.4}}
            button{font-size:90px;padding:60px 120px;margin:30px;background:#00f;color:white;border:none;border-radius:50px;cursor:pointer}
        </style>
    </head>
    <body>
        <h1>DETECTOR DE GAS - CONTROL TOTAL</h1>
        <div id="estado" class="estado limpia">CARGANDO...</div>
        <canvas id="chart"></canvas>
        <h2>ADC: <span id="valor">---</span></h2>

        <button onclick="fetch('/I')">ENCENDER LED ROJO</button>
        <button onclick="fetch('/O')" style="background:#c00">APAGAR LED</button>

        <script>
            # Configura la gráfica realtime
            const chart = new Chart(document.getElementById('chart'), {
                type:'line',
                data:{datasets:[{label:'Sensor Gas (ADC)',borderColor:'#0f0',fill:true,tension:0.4}]},
                options:{animation:false,scales:{y:{min:0,max:4095}}}
            });

            # Cada 500 ms consulta /data
            setInterval(async()=>{
                const r = await fetch("/data"); # Solicita valores al servidor
                const d = await r.json();       # Recibe JSON con datos

                chart.data.labels = d.t;        # Eje X = indices
                chart.data.datasets[0].data = d.v;  # Valores ADC
                chart.update();                 # Actualiza la gráfica

                if(d.v.length>0){               # Si hay datos válidos
                    document.getElementById("valor").textContent = d.v[d.v.length-1];  # Último ADC
                    document.getElementById("estado").textContent = d.e;               # Estado
                    document.getElementById("estado").className = "estado " + (d.e.includes("GAS")?"gas":"limpio");
                }
            }, 500);
        </script>
    </body>
    </html>
    '''

# ====================== RUTAS DE CONTROL ======================
@app.route("/I")
def I(): ser.write(b'I'); return "OK"      # Enviar comando para encender LED rojo

@app.route("/O")
def O(): ser.write(b'O'); return "OK"      # Enviar comando para apagar LED

# ====================== RUTA DE DATOS JSON ======================
@app.route("/data")
def data():
    return jsonify({
        "t": list(range(len(datos_adc))),   # índices 0..n para eje X
        "v": datos_adc.copy(),              # copia de los valores
        "e": estado_actual                  # estado textual
    })

# ====================== INICIO DEL HILO + SERVIDOR ======================
threading.Thread(target=lector_tiva, daemon=True).start()  # Arranca hilo lector UART
app.run(host="0.0.0.0", port=5000)                         # Levanta servidor Flask accesible a LAN

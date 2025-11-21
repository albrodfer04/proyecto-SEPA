# todo_en_uno_FINAL.py 
from flask import Flask, jsonify, request  # Importa Flask para web y jsonify para enviar datos en JSON
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
datos_temp = []
datos_humo = []
datos_luz = []

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

                # Buscar temperatura (acepta TEMP: o TEMPERATURA:)
                match_temp = re.search(r"TEMP(?:ERATURA)?:\s*([0-9]+(?:\.[0-9]+)?)", linea, re.IGNORECASE)
                if match_temp:
                    try:
                        tval = float(match_temp.group(1))
                        datos_temp.append(tval)
                        if len(datos_temp) > 200:
                            datos_temp.pop(0)
                    except:
                        pass

                # Buscar humo (HUMO:)
                match_humo = re.search(r"HUMO:\s*([0-9]+(?:\.[0-9]+)?)", linea, re.IGNORECASE)
                if match_humo:
                    try:
                        hval = float(match_humo.group(1))
                        datos_humo.append(hval)
                        if len(datos_humo) > 200:
                            datos_humo.pop(0)
                    except:
                        pass

                # Buscar luz (LUZ:)
                match_luz = re.search(r"LUZ:\s*([0-9]+(?:\.[0-9]+)?)", linea, re.IGNORECASE)
                if match_luz:
                    try:
                        lval = float(match_luz.group(1))
                        datos_luz.append(lval)
                        if len(datos_luz) > 200:
                            datos_luz.pop(0)
                    except:
                        pass

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
    return '''   
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="utf-8">
    <title>Control Domótico - Sensor, Ventilador y LED RGB</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body {
            background: #fff;
            color: #333;
            font-family: 'Segoe UI', Arial, sans-serif;
            text-align: center;
            padding: 40px;
        }

        h1 {
            font-size: 48px;
            margin-bottom: 20px;
            color: #0a0;
        }

        .estado {
            font-size: 48px;
            padding: 20px;
            border-radius: 20px;
            margin: 30px auto;
            width: 80%;
            max-width: 600px;
            transition: background 0.5s, color 0.5s;
        }

        .limpio {
            background: #0f0;
            color: #000;
        }

        .gas {
            background: #c00;
            color: #fff;
            animation: parp 1s infinite;
        }

        @keyframes parp {
            50% { opacity: 0.4; }
        }

        .chart-container {
            width: 32%;
            min-width: 300px;
            margin: 10px auto;
        }

        canvas {
            background: #000;
            border: 3px solid #0f0;
            border-radius: 18px;
            box-shadow: 0 0 20px rgba(0,255,0,0.5);
        }       


        h2 {
            font-size: 36px;
            margin: 20px 0;
        }

        #valor {
            font-weight: bold;
            color: #00f;
        }

        .botones, .rgb-control {
            margin-top: 40px;
            display: flex;
            justify-content: center;
            flex-wrap: wrap;
            gap: 20px;
        }

        button {
            font-size: 24px;
            padding: 15px 30px;
            border: none;
            border-radius: 20px;
            cursor: pointer;
            color: white;
            transition: background 0.3s;
        }

        button:hover {
            opacity: 0.8;
        }

        .off { background: #888; }
        .low { background: #0af; }
        .medium { background: #06c; }
        .high { background: #00f; }

        input[type="color"] {
            width: 100px;
            height: 100px;
            border: none;
            cursor: pointer;
        }

        .rgb-control button {
            background: #333;
        }

        .rgb-control button {
            font-size: 20px;
            padding: 12px 24px;
            background: #444;
            color: white;
            border: none;
            border-radius: 15px;
            cursor: pointer;
            transition: background 0.3s;
        }

        .rgb-control button:hover {
            background: #666;
        }
    </style>
</head>
<body>
    <h1>Control de la Casa en Tiempo Real</h1>

    <!-- Estado del sensor de gas -->
    <div id="estado" class="estado limpio">CARGANDO...</div>
    <div class="chart-container">
        <canvas id="chart"></canvas>
    </div>

    <h2>ADC: <span id="valor">---</span></h2>

    <!-- Control del ventilador -->
    <h2>Control de Ventilador</h2>
    <div class="botones">
        <button onclick="fetch('/')" class="off">Apagar</button>
        <button onclick="fetch('/fan/low')" class="low">Velocidad Baja</button>
        <button onclick="fetch('/fan/medium')" class="medium">Velocidad Media</button>
        <button onclick="fetch('/fan/high')" class="high">Velocidad Alta</button>
    </div>

    <!-- Control del LED RGB -->
    <h2>Control de LED RGB</h2>
    <div class="rgb-control">
        <input type="color" id="colorPicker" value="#ff0000">
        <button onclick="setColor()">Aplicar Color</button>
    </div>
    <div class="rgb-control">
        <button onclick="setPreset('off')">Apagar</button>
        <button onclick="setPreset('warm')">Luz cálida</button>
        <button onclick="setPreset('white')">Luz blanca</button>
    </div>
    

    <script>
        // Crear gradiente de fondo para la línea
        const ctx = document.getElementById('chart').getContext('2d');
        const gradient = ctx.createLinearGradient(0, 0, 0, 400);
        gradient.addColorStop(0, 'rgba(0,255,0,0.6)');
        gradient.addColorStop(1, 'rgba(0,255,0,0)');

        // Gráfica mejorada
        const chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Sensor Gas (ADC)',
                    borderColor: '#0f0',
                    backgroundColor: gradient,
                    borderWidth: 3,
                    pointRadius: 4,
                    pointHoverRadius: 8,
                    pointBackgroundColor: '#fff',
                    fill: true,
                    tension: 0.35
                }]
            },
            options: {
                animation: {
                    duration: 400,
                    easing: 'easeOutQuart'
                },
                interaction: {
                    mode: 'nearest',
                    intersect: false
                },
                plugins: {
                    legend: {
                        labels: {
                            color: '#0f0',
                            font: { size: 18 }
                        }
                    },
                    tooltip: {
                        backgroundColor: '#000',
                        titleColor: '#0f0',
                        bodyColor: '#fff',
                        borderColor: '#0f0',
                        borderWidth: 1,
                        padding: 12
                    }
                },
                scales: {
                    x: {
                        ticks: { color: '#0f0' },
                        grid: { color: 'rgba(0,255,0,0.2)' }
                    },
                    y: {
                        min: 0,
                        max: 4095,
                        ticks: { color: '#0f0' },
                        grid: { color: 'rgba(0,255,0,0.2)' }
                    }
                }
            }
        });


        // Actualiza datos del sensor cada 500 ms
        setInterval(async () => {
            const r = await fetch("/data");
            const d = await r.json();

            chart.data.labels = d.t;
            chart.data.datasets[0].data = d.v;
            chart.update();

            if (d.v.length > 0) {
                document.getElementById("valor").textContent = d.v[d.v.length - 1];
                document.getElementById("estado").textContent = d.e;
                document.getElementById("estado").className = "estado " + (d.e.includes("GAS") ? "gas" : "limpio");
            }
        }, 500);

        // Control del LED RGB
        function setColor() {
            const color = document.getElementById("colorPicker").value;
            fetch(`/rgb?color=${color}`);
        }
        function setPreset(type) {
            let color = "#000000"; // apagado

            if (type === "warm") color = "#FFD080";   // luz cálida
            if (type === "white") color = "#FFFFFF";  // luz blanca

            fetch(`/rgb?color=${color}`);
        }
    </script>
</body>
</html>
    '''

# ====================== RUTAS DE CONTROL ======================
@app.route("/fan/off")
def fan_off():
    ser.write(b'F')  # Comando para apagar ventilador
    return "Ventilador apagado"

@app.route("/fan/low")
def fan_low():
    ser.write(b'L')  # Comando para velocidad baja
    return "Ventilador velocidad baja"

@app.route("/fan/medium")
def fan_medium():
    ser.write(b'M')  # Comando para velocidad media
    return "Ventilador velocidad media"

@app.route("/fan/high")
def fan_high():
    ser.write(b'H')  # Comando para velocidad alta
    return "Ventilador velocidad alta"

@app.route("/rgb")
def rgb():
    color = request.args.get("color", "#000000")  # valor por defecto negro
    try:
        r = int(color[1:3], 16)
        g = int(color[3:5], 16)
        b = int(color[5:7], 16)
        comando = f'R{r:03}G{g:03}B{b:03}'  # Ejemplo: R255G000B128
        ser.write(comando.encode())
        return f"Color enviado: {comando}"
    except:
        return "Color inválido", 400


# ====================== RUTA DE DATOS JSON ======================
@app.route("/data")
def data():
    # Construir eje X con la longitud máxima entre series
    max_len = max(len(datos_adc), len(datos_temp), len(datos_humo), len(datos_luz), 1)
    return jsonify({
        "t": list(range(max_len)),          # etiquetas X (índices)
        "v": datos_adc.copy(),              # sensor gas/ADC (compatible con versión anterior)
        "temp": datos_temp.copy(),          # temperatura
        "humo": datos_humo.copy(),          # humo
        "luz": datos_luz.copy(),            # luz
        "e": estado_actual                  # estado textual
    })

# ====================== INICIO DEL HILO + SERVIDOR ======================
threading.Thread(target=lector_tiva, daemon=True).start()  # Arranca hilo lector UART
app.run(host="0.0.0.0", port=5000)                         # Levanta servidor Flask accesible a LAN

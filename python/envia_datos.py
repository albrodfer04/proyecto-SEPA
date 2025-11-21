# todo_en_uno_FINAL.py
from flask import Flask, jsonify, request
import serial
import threading
import time
import re

app = Flask(__name__)

PUERTO = "COM6"
ARCHIVO = r"C:\Users\aledi\Desktop\live_data.txt"

# Abrir puerto serial
ser = serial.Serial(PUERTO, 115200, timeout=0.1)
print(f"Conectado a {PUERTO} – esperando datos de la Tiva...")

# Buffers y variables globales
datos_adc = []
datos_temp = []
datos_humo = []
datos_luz = []
puerta = "PA"
ayuda = 0
sos = 0
comida = 0
past = 0
persona = 0
estado_actual = "CARGANDO..."

# ====================== HILO LECTOR SERIAL ======================
def lector_tiva():
    global datos_adc, datos_temp, datos_luz, estado_actual
    global ayuda, sos, comida, past, persona, puerta

    while True:
        if ser.in_waiting > 0:
            try:
                linea = ser.readline().decode('utf-8', errors='ignore').strip()
                if not linea:
                    continue

                print(linea)

                with open(ARCHIVO, "a", encoding="utf-8") as f:
                    f.write(linea + "\n")

                regex_all = re.compile(
                    r"GAS:(\d+).*?(PA|PC).*?LUZ:\s*(\d+).*?T:\s*([0-9]+(?:\.[0-9]+)?)"
                    r".*?A:\s*(\d+).*?SOS:\s*(\d+).*?C:\s*(\d+).*?Past:\s*(\d+).*?Pers:\s*(\d+)",
                    re.IGNORECASE
                )

                match = regex_all.search(linea)
                if match:
                    gas     = int(match.group(1))
                    puerta  = match.group(2)
                    luz     = int(match.group(3))
                    temp    = float(match.group(4))
                    ayuda   = int(match.group(5))
                    sos     = int(match.group(6))
                    comida  = int(match.group(7))
                    past    = int(match.group(8))
                    persona = int(match.group(9))

                    datos_adc.append(gas)
                    datos_temp.append(temp)
                    datos_luz.append(luz)

                    if len(datos_adc) > 200: datos_adc.pop(0)
                    if len(datos_temp) > 200: datos_temp.pop(0)
                    if len(datos_luz) > 200: datos_luz.pop(0)

                    estado_actual = "GAS DETECTADO" if gas > 800 else "AIRE LIMPIO"

            except:
                pass
        time.sleep(0.01)

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
body { background: #fff; color: #333; font-family: 'Segoe UI', Arial, sans-serif; text-align: center; padding: 40px; }
h1 { font-size: 48px; margin-bottom: 20px; color: #0a0; }

.estado {
    font-size: 48px;
    padding: 20px;
    border-radius: 20px;
    margin: 20px auto 40px auto;
    width: 80%;
    max-width: 600px;
    transition: background 0.5s, color 0.5s;
}
.limpio { background: #0f0; color: #000; }
.gas { background: #c00; color: #fff; animation: parp 1s infinite; }
@keyframes parp { 50% { opacity:0.4; } }

.charts { display: flex; justify-content: center; gap: 25px; flex-wrap: wrap; margin-bottom: 40px; }
canvas { width: 350px !important; height: 260px !important; background: #000; border: 3px solid #0f0; border-radius: 15px; }

.botones, .rgb-control { margin-top: 40px; display: flex; justify-content: center; flex-wrap: wrap; gap: 20px; }
button { font-size: 24px; padding: 15px 30px; border: none; border-radius: 20px; cursor: pointer; color: white; transition: 0.3s; }
button:hover { opacity: 0.8; }
.off { background: #888; }
.low { background: #0af; }
.medium { background: #06c; }
.high { background: #00f; }
.rgb-control button { background: #333; font-size: 20px; padding: 12px 24px; border-radius: 15px; }
.rgb-control button:hover { background: #666; }
input[type="color"] { width: 100px; height: 100px; border: none; cursor: pointer; }
.alert-buttons { display: flex; justify-content: center; gap: 20px; margin-bottom: 30px; }
.ayuda { background: #f90; }
.sos { background: #c00; }
</style>
</head>
<body>
<h1>Control de la Casa en Tiempo Real</h1>

<div id="estado" class="estado limpio">CARGANDO...</div>

<div class="alert-buttons">
    <button class="ayuda" onclick="sendAlert('ayuda')">AYUDA</button>
    <button class="sos" onclick="sendAlert('sos')">SOS</button>
</div>

<div class="charts">
    <canvas id="chartGas"></canvas>
    <canvas id="chartLuz"></canvas>
    <canvas id="chartTemp"></canvas>
</div>

<h2>Control de Ventilador</h2>
<div class="botones">
    <button onclick="fetch('/fan/off')" class="off">Apagar</button>
    <button onclick="fetch('/fan/low')" class="low">Velocidad Baja</button>
    <button onclick="fetch('/fan/medium')" class="medium">Velocidad Media</button>
    <button onclick="fetch('/fan/high')" class="high">Velocidad Alta</button>
</div>

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
const gGas = new Chart(document.getElementById('chartGas'), { type:'line', data:{labels:[], datasets:[{label:"GAS", borderColor:"#0f0", data:[]}]}} );
const gLuz = new Chart(document.getElementById('chartLuz'), { type:'line', data:{labels:[], datasets:[{label:"LUZ", borderColor:"#ff0", data:[]}]}} );
const gTemp = new Chart(document.getElementById('chartTemp'), { type:'line', data:{labels:[], datasets:[{label:"TEMP", borderColor:"#0af", data:[]}]} } );

setInterval(async ()=>{
    const r = await fetch("/data"); const d = await r.json();
    gGas.data.labels = d.t; gGas.data.datasets[0].data = d.v; gGas.update();
    gLuz.data.labels = d.t; gLuz.data.datasets[0].data = d.luz; gLuz.update();
    gTemp.data.labels = d.t; gTemp.data.datasets[0].data = d.temp; gTemp.update();

    document.getElementById("estado").textContent = d.e;
    document.getElementById("estado").className = "estado " + (d.e.includes("GAS") ? "gas" : "limpio");
},500);

async function sendAlert(tipo){
    try {
        const res = await fetch(`/${tipo}`);
        const text = await res.text();
        alert(text);
        setTimeout(()=>fetch("/reset_alert?tipo=" + tipo),2000);
    } catch(err){ console.error(err); }
}

function setColor(){
    const c = document.getElementById("colorPicker").value;
    fetch(`/rgb?color=${c}`);
}
function setPreset(t){
    let c="#000000"; if(t=="warm") c="#FFD080"; if(t=="white") c="#FFFFFF";
    fetch(`/rgb?color=${c}`);
}
</script>
</body>
</html>
'''

# ====================== RUTAS DE CONTROL ======================
@app.route("/reset_alert")
def reset_alert():
    global ayuda, sos
    tipo = request.args.get("tipo")
    if tipo == "ayuda": ayuda = 0
    elif tipo == "sos": sos = 0
    return "OK"

@app.route("/ayuda")
def enviar_ayuda():
    global ayuda
    ser.write(b'I')
    ayuda = 1
    return "AYUDA enviada"

@app.route("/sos")
def enviar_sos():
    global sos
    ser.write(b'S')
    sos = 1
    return "SOS enviada"

@app.route("/fan/off")
def fan_off():
    ser.write(b'0')
    return "Ventilador apagado"

@app.route("/fan/low")
def fan_low():
    ser.write(b'L')
    return "Ventilador velocidad baja"

@app.route("/fan/medium")
def fan_medium():
    ser.write(b'R')
    return "Ventilador velocidad media"

@app.route("/fan/high")
def fan_high():
    ser.write(b'B')
    return "Ventilador velocidad alta"

@app.route("/rgb")
def rgb():
    color = request.args.get("color", "#000000")
    try:
        r = int(color[1:3],16)
        g = int(color[3:5],16)
        b = int(color[5:7],16)
        comando = f'R{r:03}G{g:03}B{b:03}'
        ser.write(comando.encode())
        return f"Color enviado: {comando}"
    except:
        return "Color inválido",400

# ====================== RUTA DE DATOS JSON ======================
@app.route("/data")
def data():
    max_len = max(len(datos_adc), len(datos_temp), len(datos_humo), len(datos_luz), 1)
    return jsonify({
        "t": list(range(max_len)),
        "v": datos_adc.copy(),
        "temp": datos_temp.copy(),
        "luz": datos_luz.copy(),
        "e": estado_actual,
        "puerta": puerta,
        "ayuda": ayuda,
        "sos": sos,
        "comida": comida,
        "pastillas": past,
        "persona": persona
    })

# ====================== INICIO ======================
threading.Thread(target=lector_tiva, daemon=True).start()
app.run(host="0.0.0.0", port=5000)

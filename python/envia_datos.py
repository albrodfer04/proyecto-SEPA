# todo_en_uno_FINAL.py
# Proyecto de control domótico en tiempo real con Flask y Tiva
# Autor: estudiante de 4º ingeniería 😎

from flask import Flask, jsonify, request  # para la web y los endpoints
import serial  # comunicación con Tiva por puerto serie
import threading  # para leer el serial en paralelo
import time
import re  # expresiones regulares para parsear las líneas

# ==== CREO LA APP DE FLASK ====
mi_app = Flask(__name__)

# ==== CONFIG SERIAL Y ARCHIVO ====
mi_puerto = "COM4"  # puerto donde está la Tiva
mi_archivo = r"C:\Users\aledi\Desktop\live_data.txt"  # donde guardamos los datos

# abrir puerto serie con baudrate 115200
puerto_serial = serial.Serial(mi_puerto, 115200, timeout=0.1)
print(f"Conectado a {mi_puerto} – esperando datos de la Tiva...")

# ==== BUFFERS PARA GUARDAR DATOS Y VARIABLES GLOBALES ====
gas_adcc = []
temp = []
luz = []

puerta = "PA"
ayuda = 0
sos = 0
comida = 0
pastillas = 0
persona = 0
estado_actual = "CARGANDO..."  # estado inicial

# ====================== HILO QUE LEE EL SERIAL ======================
def hilo_lector_tiva():
    """Hilo que corre en paralelo y lee las líneas que manda la Tiva por serial"""
    global gas_adcc, temp, luz, estado_actual
    global ayuda, sos, comida, pastillas, persona, puerta

    while True:
        if puerto_serial.in_waiting > 0:
            try:
                linea = puerto_serial.readline().decode('utf-8', errors='ignore').strip()
                if not linea:
                    continue  # si viene vacía, seguimos

                print(linea)  # imprimimos para debug

                # guardamos la línea en un archivo para registros
                with open(mi_archivo, "a", encoding="utf-8") as f:
                    f.write(linea + "\n")

                # regex para extraer todos los valores
                patron = re.compile(
                    r"GAS:(\d+).*?(PA|PC).*?LUZ:\s*(\d+).*?T:\s*([0-9]+(?:\.[0-9]+)?)"
                    r".*?A:\s*(\d+).*?SOS:\s*(\d+).*?C:\s*(\d+).*?Past:\s*(\d+).*?Pers:\s*(\d+)",
                    re.IGNORECASE
                )

                match = patron.search(linea)
                if match:
                    gas_valor     = int(match.group(1))
                    puerta = match.group(2)
                    luz_valor     = int(match.group(3))
                    temp_valor    = float(match.group(4))
                    ayuda    = int(match.group(5))
                    sos      = int(match.group(6))
                    comida   = int(match.group(7))
                    pastillas= int(match.group(8))
                    persona  = int(match.group(9))

                    # agregamos los datos a los buffers
                    gas_adcc.append(gas_valor)
                    temp.append(temp_valor)
                    luz.append(luz_valor)

                    # mantenemos solo los últimos 200 datos para que no explote la memoria
                    if len(gas_adcc) > 200: gas_adcc.pop(0)
                    if len(temp) > 200: temp.pop(0)
                    if len(luz) > 200: luz.pop(0)

                    # determinamos el estado del aire según el gas
                    estado_actual = "GAS DETECTADO" if gas_valor > 800 else "AIRE LIMPIO"

            except:
                pass  # ignoramos cualquier error de parseo
        time.sleep(0.01)  # dormimos un poquito para no saturar el CPU

# ====================== RUTA PRINCIPAL DE LA WEB ======================
@mi_app.route("/")
def pagina_web():
    # aquí va todo el HTML/JS/CSS embebido
    return '''
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<title>Control Domótico - Sensor, Ventilador y LED RGB</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<style>
/* ================== ESTILOS GENERALES ================== */
body { background: #fff; color: #333; font-family: 'Segoe UI', Arial, sans-serif; text-align: center; padding: 40px; }
h1 { font-size: 48px; margin-bottom: 20px; color: #0a0; }

/* ===== ESTADO DEL SENSOR DE GAS ===== */
.estado {
    font-size: 48px;
    padding: 20px;
    border-radius: 20px;
    margin: 20px auto 40px auto;
    width: 80%;
    max-width: 600px;
    transition: background 0.5s, color 0.5s;
}
.limpio { background: #0f0; color: #000; } /* aire limpio */
.gas { background: #c00; color: #fff; animation: parp 1s infinite; } /* gas detectado parpadeando */
@keyframes parp { 50% { opacity:0.4; } }

/* ===== GRÁFICAS ===== */
.charts { display: flex; justify-content: center; gap: 25px; flex-wrap: wrap; margin-bottom: 40px; }
canvas { width: 350px !important; height: 260px !important; background: #FAFAFA; border: 3px solid #DEDEDE; border-radius: 15px; }

/* ===== BOTONES ===== */
.alert-buttons {
    margin-top: 25px;
    display: flex;
    justify-content: center;
    gap: 25px;      /* separación entre botones */
    flex-wrap: wrap; /* se adaptan cuando no hay espacio */
}

.botones, .rgb-control { margin-top: 40px; display: flex; justify-content: center; flex-wrap: wrap; gap: 20px; }
button { font-size: 24px; padding: 15px 30px; border: none; border-radius: 20px; cursor: pointer; color: white; transition: 0.3s; }
button:hover { opacity: 0.8; }

/* colores de botones de ventilador */
.off { background: #888; }
.low { background: #0af; }
.medium { background: #06c; }
.high { background: #00f; }

/* niveles de luz en amarillo */
.luz-off { background: #888; }       /* apagado: gris */
.luz-media { background: #fbc02d; color: #fff; } /* amarillo medio */
.luz-alta { background: #FFB500; color: #fff; }  /* amarillo oscuro */

/* Botones de alerta, colores chulos y distintivos */
.btn-ayuda { background: #4CAF50; color: #000 ; }   /* verde */
.btn-sos { background: #4CAF50; color: #000 ; }       /* verde */
.btn-pastillas { background: #FF8FB1 ; color: #000; } /* rosa */
.btn-comida { background: #FF8FB1 ; color: #000; }   /* rosa */

/* ALERTAS DE COMIDA Y PASTILLAS – ESTILO COHERENTE */
.alert-reminder {
    display: none;                  /* solo visible cuando toca */
    background: #fff0f5;            /* fondo lila muy suave */
    color: #6a0dad;                 /* texto morado, como los botones de persiana */
    font-size: 28px;                 /* tamaño visible pero elegante */
    font-weight: bold;
    padding: 20px 30px;
    margin: 20px auto;
    width: 60%;
    border-radius: 15px;
    border: 2px solid #FF8FB1;  /* rosa suave */
    text-align: center;
    box-shadow: 0 4px 10px rgba(0,0,0,0.1);
    transition: background 0.3s, color 0.3s;
}


/* BOTONES PERSIANA MORADOS */
.persiana-up  { background: #A582B8; color: #fff; }   /* morado claro */
.persiana-mid { background: #8E56BA; color: #fff; }   /* morado medio */
.persiana-down{ background: #7535B0; color: #fff; }   /* morado oscuro */

</style>
</head>
<body>
<h1>Control de la Casa en Tiempo Real</h1>

<!-- DIV PRINCIPAL DE ESTADO -->
<div id="estado" class="estado limpio">CARGANDO...</div>

<!-- BOTONES DE ALERTA -->
<div class="alert-buttons">
    <button class="btn-ayuda" onclick="sendAlert('ayuda')">ENVIAR AYUDA</button> <!-- ayuda -->
    <button class="btn-sos" onclick="sendAlert('sos')">SOS OK</button>             <!-- SOS -->
</div>

<div class="alert-buttons">
    <button class="btn-pastillas" onclick="sendAlert('pastillas')">RECORDAR PASTILLAS</button> <!-- pastillas -->
    <button class="btn-comida" onclick="sendAlert('comida')">RECORDAR COMIDA</button>         <!-- comida -->
</div>

<!-- ALERTAS VISIBLES -->
<div id="alertPast" class="alert-reminder"> No ha tomado pastillas</div>
<div id="alertComida" class="alert-reminder">🍽 No ha comido</div>

<br><br>

<!-- GRAFICAS DE DATOS EN TIEMPO REAL -->
<div class="charts">
    <canvas id="chartGas"></canvas>
    <canvas id="chartLuz"></canvas>
    <canvas id="chartTemp"></canvas>
</div>

<h2>Control de Ventilador</h2>
<div class="botones">
    <!-- botones de ventilador, cada uno envía petición fetch -->
    <button onclick="sendAlert('fan/off')" class="off">Apagar</button>
    <button onclick="sendAlert('fan/low')" class="low">Velocidad Baja</button>
    <button onclick="sendAlert('fan/medium')" class="medium">Velocidad Media</button>
    <button onclick="sendAlert('fan/high')" class="high">Velocidad Alta</button>
</div>

<h2>Control de Luz</h2>
<div class="botones">
    <button class="luz-off" onclick="sendAlert('luz/off')">LUZ OFF</button>
    <button class="luz-media" onclick="sendAlert('luz/medium')">LUZ MEDIA</button>
    <button class="luz-alta" onclick="sendAlert('luz/high')">LUZ ALTA</button>
</div>

<h2>Control de Persiana</h2>
<div class="botones">
    <button class="persiana-up" onclick="sendAlert('blind/up')">BAJADA</button>
    <button class="persiana-mid" onclick="sendAlert('blind/mid')">MEDIA</button>
    <button class="persiana-down" onclick="sendAlert('blind/down')">SUBIDA</button>
</div>

<script>
// ===================== GRÁFICAS CON TIEMPO REAL =====================
const ctxGas  = document.getElementById('chartGas');
const ctxLuz  = document.getElementById('chartLuz');
const ctxTemp = document.getElementById('chartTemp');

const opciones = {
    type: 'line',
    options: {
        responsive: true,
        animation: false,
        plugins: { legend: { position: 'top' } },
        scales: {
            x: { ticks: { maxRotation: 0, maxTicksLimit: 8 }, grid: { display: false } },
            y: { beginAtZero: true }
        }
    }
};

// Crear las 3 gráficas (muy corto)
const charts = {
    gas:  new Chart(ctxGas,  { ...opciones, data: { labels: [], datasets: [{ label: 'Gas',  data: [], borderColor: '#2196F3', tension: 0.3 }] }, options: { ...opciones.options, scales: { ...opciones.options.scales, y: { max: 4095 } } } }),
    luz:  new Chart(ctxLuz,  { ...opciones, data: { labels: [], datasets: [{ label: 'Luz',  data: [], borderColor: '#FF9800', tension: 0.3 }] }, options: { ...opciones.options, scales: { ...opciones.options.scales, y: { max: 1023 } } } }),
    temp: new Chart(ctxTemp, { ...opciones, data: { labels: [], datasets: [{ label: 'Temp °C', data: [], borderColor: '#F44336', tension: 0.3 }] }, options: { ...opciones.options, scales: { ...opciones.options.scales, y: { min: 15, max: 40 } } } })
};

// ===================== ACTUALIZACIÓN CADA 500ms (¡SÚPER SIMPLE!) =====================
setInterval(async () => {
    const d = await (await fetch("/data")).json();
    const hora = new Date().toLocaleTimeString('es-ES', { hour:'2-digit', minute:'2-digit', second:'2-digit' });

    // Actualizar estado y alertas
    const estado = document.getElementById("estado");
    estado.textContent = d.sos ? "SOS ACTIVADO" : d.ayuda ? "AYUDA PEDIDA" : d.e;
    estado.className = "estado " + (d.sos || d.ayuda || d.e.includes("GAS") ? "gas" : "limpio");

    document.getElementById("alertPast").style.display = d.pastillas ? "block" : "none";
    document.getElementById("alertComida").style.display = d.comida ? "block" : "none";

    // Añadir nuevo punto a las 3 gráficas
    const ultimo = { x: hora, y: 0 };
    charts.gas.data.labels.push(hora);   charts.gas.data.datasets[0].data.push(d.v.at(-1)  || 0);
    charts.luz.data.labels.push(hora);   charts.luz.data.datasets[0].data.push(d.luz.at(-1) || 0);
    charts.temp.data.labels.push(hora);  charts.temp.data.datasets[0].data.push(d.temp.at(-1)|| 0);

    // Mantener solo 200 puntos
    if (charts.gas.data.labels.length > 200) {
        ["gas","luz","temp"].forEach(t => {
            charts[t].data.labels.shift();
            charts[t].data.datasets[0].data.shift();
        });
    }

    // Actualizar (sin animación)
    Object.values(charts).forEach(c => c.update('none'));
}, 500);

// ===================== ENVÍO DE ALERTAS =====================
async function sendAlert(tipo) {
    try { alert(await (await fetch(`/${tipo}`)).text()); }
    catch(e) { console.error(e); }
}
</script>
</body>
</html>
'''
# ====================== RUTAS DE CONTROL ======================

@mi_app.route("/ayuda")
def enviar_ayuda():
    puerto_serial.write(b'A')   # micro decide qué hacer
    return "AYUDA ENVIADA"

@mi_app.route("/sos")
def enviar_sos():
    puerto_serial.write(b'S')
    return "SOS ENVIADO"

@mi_app.route("/pastillas")
def enviar_pastillas():
    puerto_serial.write(b'P')
    return "Aviso de pastillas enviado"

@mi_app.route("/comida")
def enviar_comida():
    puerto_serial.write(b'C')
    return "Aviso de comida enviado"

# ==== CONTROL VENTILADOR ====
@mi_app.route("/fan/off")
def fan_apagar():
    puerto_serial.write(b'O')
    return "Ventilador apagado"

@mi_app.route("/fan/low")
def fan_baja():
    puerto_serial.write(b'L')
    return "Ventilador velocidad baja"

@mi_app.route("/fan/medium")
def fan_media():
    puerto_serial.write(b'R')
    return "Ventilador velocidad media"

@mi_app.route("/fan/high")
def fan_alta():
    puerto_serial.write(b'B')
    return "Ventilador velocidad alta"

# ==== CONTROL LUZ ====
@mi_app.route("/luz/off")
def luz_apagar():
    puerto_serial.write(b'N')  # comando Tiva luz OFF
    return "Luz apagada"

@mi_app.route("/luz/medium")
def luz_media():
    puerto_serial.write(b'I')  # comando Tiva luz media
    return "Luz media"

@mi_app.route("/luz/high")
def luz_alta():
    puerto_serial.write(b'M')  # comando Tiva luz alta
    return "Luz alta"

# === CONTROL PERSIANAS
@mi_app.route("/blind/up")
def persiana_subir():
    puerto_serial.write(b'U')
    return "Persiana subiendo"

@mi_app.route("/blind/mid")
def persiana_medio():
    puerto_serial.write(b'M')
    return "Persiana a mitad"

@mi_app.route("/blind/down")
def persiana_bajar():
    puerto_serial.write(b'D')
    return "Persiana bajando"


# ====================== RUTA DE DATOS JSON ======================
@mi_app.route("/data")
def endpoint_datos():
    return jsonify({
        "v": gas_adcc.copy(),
        "luz": luz.copy(),
        "temp": temp.copy(),
        "e": estado_actual,
        "ayuda": ayuda,
        "sos": sos,
        "comida": comida,
        "pastillas": pastillas
    })

# ====================== INICIO DEL HILO Y DEL SERVIDOR ======================
threading.Thread(target=hilo_lector_tiva, daemon=True).start()
mi_app.run(host="0.0.0.0", port=5000)

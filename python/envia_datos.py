# LEE EL PUERTO SERIE Y ESCRIBE CON EL, INTERACCION WEB

from flask import Flask, jsonify, request  # para la web y los endpoints
import serial  # comunicacion con Tiva por puerto serie
import threading  # para leer el serial en paralelo
import time
import re  # expresiones regulares para parsear las lineas

# CREO LA APP DE FLASK 
mi_app = Flask(__name__)

mi_puerto = "COM4"  # puerto serie
mi_archivo = r"C:\Users\aledi\Desktop\live_data.txt"  # donde guardamos los datos

# ABRIR PUERTO SERIE
puerto_serial = serial.Serial(mi_puerto, 115200, timeout=0.1)
print(f"Conectado a {mi_puerto} - esperando datos de la Tiva...")

# VARIABLES GLOBALES
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

# HILO QUE LEE EL SERIAL 
def hilo_lector_tiva():
    """Hilo que corre en paralelo y lee las lineas que manda la Tiva por serial"""
    global gas_adcc, temp, luz, estado_actual
    global ayuda, sos, comida, pastillas, persona, puerta

    while True:
        if puerto_serial.in_waiting > 0:
            try:
                linea = puerto_serial.readline().decode('utf-8', errors='ignore').strip()
                if not linea:
                    continue  # si viene vacia, seguimos

                print(linea)  # imprimimos para debug por el cmd

                # guardamos la linea en un archivo para registros
                with open(mi_archivo, "a", encoding="utf-8") as f:
                    f.write(linea + "\n")

                # regex para extraer todos los valores
                patron = re.compile(
                    r"GAS:(\d+).*?(PA|PC).*?LUZ:\s*(\d+).*?T:\s*([0-9]+(?:\.[0-9]+)?)"
                    r".*?A:\s*(\d+).*?SOS:\s*(\d+).*?C:\s*(\d+).*?Past:\s*(\d+).*?Pers:\s*(\d+)",
                    re.IGNORECASE
                )

                # ponemos daros en las variables
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

                    # determinamos el estado del aire segun el valor adc del gas
                    estado_actual = "GAS DETECTADO" if gas_valor > 1000 else "AIRE LIMPIO"

            except:
                pass  # ignoramos cualquier error de parseo
        time.sleep(0.01)  # dormimos un poquito para no saturar el CPU

# RUTA PRINCIPAL DE LA WEB 
@mi_app.route("/")
def pagina_web():
    # aqui va todo el HTML/JS/CSS 
    return
'''
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<title>Control Domótico - Sensor, Ventilador y LED RGB</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script> <!-- Chart.js para las gráficas en tiempo real -->
<style>
/* todo el fondo blanco y letra oscura, centrado */
body { background:#fff; color:#333; font-family:Arial,sans-serif; text-align:center; padding:40px; }
h1 { font-size:48px; margin-bottom:20px; color:#0a0; }

/* cuadro grande que dice si hay gas o está todo bien */
.estado { 
    font-size:48px; padding:20px; border-radius:20px; margin:20px auto 40px; width:80%; max-width:600px;
    transition:background .5s,color .5s;
}
.limpio { background:#0f0; color:#000; }           /* verde = aire limpio */
.gas { background:#c00; color:#fff; animation:parp 1s infinite; } /* rojo + parpadeo = gas detectado */
@keyframes parp { 50% { opacity:.4; }}

/* gráficas, se ponen en fila o columna según pantalla */
.charts { display:flex; justify-content:center; gap:25px; flex-wrap:wrap; margin-bottom:40px; }
canvas { width:350px !important; height:260px !important; background:#fafafa; border:3px solid #dedede; border-radius:15px; }

/* filas de botones grandes */
.alert-buttons { margin-top:25px; display:flex; justify-content:center; gap:25px; flex-wrap:wrap; }
.botones { margin-top:40px; display:flex; justify-content:center; flex-wrap:wrap; gap:20px; }

/* estilo común de todos los botones */
button { font-size:24px; padding:15px 30px; border:none; border-radius:20px; cursor:pointer; color:white; transition:.3s; }
button:hover { opacity:.8; }

/* colores ventilador */
.off { background:#888; }      /* gris = apagado */
.low { background:#0af; }      /* azul claro */
.medium { background:#06c; }   /* azul medio */
.high { background:#00f; }     /* azul fuerte */

/* colores luz */
.luz-off { background:#888; }
.luz-media { background:#fbc02d; }   /* amarillo suave */
.luz-alta { background:#ffb500; }    /* naranja fuerte */

/* colores botones de emergencia y recordatorios */
.btn-ayuda,.btn-sos { background:#4caf50; color:#000; }        /* verde */
.btn-pastillas,.btn-comida { background:#ff8fb1; color:#000; } /* rosa */

/* cartelitos de recordatorio de comer/tomar pastillas */
.alert-reminder {
    display:none; background:#fff0f5; color:#6a0dad; font-size:28px; font-weight:bold;
    padding:20px 30px; margin:20px auto; width:60%; border-radius:15px; border:2px solid #ff8fb1;
    text-align:center; box-shadow:0 4px 10px rgba(0,0,0,.1);
}

/* botones de la persiana (morados) */
.persiana-up { background:#a582b8; }     /* subir persiana */
.persiana-mid { background:#8e56ba; }    /* posición media */
.persiana-down { background:#7535b0; }   /* bajar persiana */
</style>
</head>
<body>
<h1>Control de la Casa en Tiempo Real</h1>

<!-- aquí sale "AIRE LIMPIO" o "GAS DETECTADO!!" o SOS -->
<div id="estado" class="estado limpio">CARGANDO...</div>

<!-- cartel grande de puerta y si hay persona dentro -->
<div style="display:flex; justify-content:center; gap:40px; margin:30px 0; flex-wrap:wrap;">
    <div id="cartelPuerta" style="padding:25px 40px; border-radius:20px; font-size:36px; font-weight:bold; color:white; min-width:280px; box-shadow:0 6px 15px rgba(0,0,0,.2);">
        PUERTA: CARGANDO...
    </div>
    <div id="cartelPersona" style="padding:25px 40px; border-radius:20px; font-size:36px; font-weight:bold; color:white; min-width:280px; box-shadow:0 6px 15px rgba(0,0,0,.2);">
        PERSONA: CARGANDO...
    </div>
</div>

<!-- botones de emergencia -->
<div class="alert-buttons">
    <button class="btn-ayuda" onclick="sendAlert('ayuda')">ENVIAR AYUDA</button>
    <button class="btn-sos" onclick="sendAlert('sos')">SOS OK</button>
</div>

<!-- botones recordatorios -->
<div class="alert-buttons">
    <button class="btn-pastillas" onclick="sendAlert('pastillas')">RECORDAR PASTILLAS</button>
    <button class="btn-comida" onclick="sendAlert('comida')">RECORDAR COMIDA</button>
</div>

<!-- mensajes que aparecen cuando el ESP dice que toca comer/pastillas -->
<div id="alertPast" class="alert-reminder"> No ha tomado pastillas</div>
<div id="alertComida" class="alert-reminder">No ha comido</div>

<br><br>

<!-- gráficas en tiempo real -->
<div class="charts">
    <canvas id="chartGas"></canvas>    <!-- valor del MQ-2 (0-1023) -->
    <canvas id="chartLuz"></canvas>    <!-- fotoresistor o LDR -->
    <canvas id="chartTemp"></canvas>   <!-- sensor de temperatura (ºC) -->
</div>

<h2>Control de Ventilador</h2>
<div class="botones">
    <button onclick="sendAlert('fan/off')" class="off">Apagar</button>
    <button onclick="sendAlert('fan/low')" class="low">Baja</button>
    <button onclick="sendAlert('fan/medium')" class="medium">Media</button>
    <button onclick="sendAlert('fan/high')" class="high">Alta</button>
</div>

<h2>Control de Luz</h2>
<div class="botones">
    <button class="luz-off" onclick="sendAlert('luz/off')">OFF</button>
    <button class="luz-media" onclick="sendAlert('luz/medium')">MEDIA</button>
    <button class="luz-alta" onclick="sendAlert('luz/high')">ALTA</button>
</div>

<h2>Control de Persiana</h2>
<div class="botones">
    <button class="persiana-up" onclick="sendAlert('blind/up')">BAJADA</button>   
    <button class="persiana-mid" onclick="sendAlert('blind/mid')">MEDIA</button>
    <button class="persiana-down" onclick="sendAlert('blind/down')">SUBIDA</button>
</div>

<script>
// ---- CANVAS ----
const ctxGas  = document.getElementById('chartGas');   // gráfica gas
const ctxLuz  = document.getElementById('chartLuz');   // gráfica luz
const ctxTemp = document.getElementById('chartTemp');  // gráfica temperatura

// configuración común de todas las gráficas
const base = {
    type:'line',
    options:{
        responsive:true,
        animation:false,                     // sin animaciones
        plugins:{legend:{position:'top'}},
        scales:{
            x:{ticks:{maxRotation:0,maxTicksLimit:8},grid:{display:false}},
            y:{beginAtZero:true}
        }
    }
};

// creo las 3 gráficas con sus límites de eje Y
const charts = {
    gas:  new Chart(ctxGas,  { ...base, 
        data:{labels:[],datasets:[{label:'Gas (MQ)',data:[],borderColor:'#2196F3',tension:.3}]},
        options:{...base.options,scales:{...base.options.scales,y:{max:1023}}}}), 
    
    luz:  new Chart(ctxLuz,  { ...base, 
        data:{labels:[],datasets:[{label:'Luz',data:[],borderColor:'#FF9800',tension:.3}]},
        options:{...base.options,scales:{...base.options.scales,y:{max:4000}}}}), 
    
    temp: new Chart(ctxTemp, { ...base, 
        data:{labels:[],datasets:[{label:'Temp °C',data:[],borderColor:'#F44336',tension:.3}]},
        options:{...base.options,scales:{...base.options.scales,y:{min:10,max:40}}}}) 
};

// ---- REFRESCO CADA 500ms ----
setInterval(async()=> {
    const d = await (await fetch("/data")).json();                 // pido JSON 
    const hora = new Date().toLocaleTimeString('es-ES',{hour:'2-digit',minute:'2-digit',second:'2-digit'}); // HH:MM:SS

    // cuadro principal de estado
    const estado = document.getElementById("estado");
    estado.textContent = d.sos ? "SOS ACTIVADO" : d.ayuda ? "AYUDA PEDIDA" : d.e; // d.e es el texto del ESP ("AIRE LIMPIO" o "GAS!!")
    estado.className = "estado " + (d.sos || d.ayuda || d.e.includes("GAS") ? "gas" : "limpio");

    // cartel puerta (rojo = abierta, verde = cerrada)
    const cartelPuerta = document.getElementById("cartelPuerta");
    cartelPuerta.textContent = `PUERTA: ${d.puerta}`;
    cartelPuerta.style.backgroundColor = d.puerta === "ABIERTA" ? "#e53935" : "#43a047";

    // cartel persona (verde = dentro, gris = fuera)
    const cartelPersona = document.getElementById("cartelPersona");
    cartelPersona.textContent = `PERSONA: ${d.persona}`;
    cartelPersona.style.backgroundColor = d.persona === "DENTRO" ? "#43a047" : "#b0b0b0";
    cartelPersona.style.color = d.persona === "DENTRO" ? "white" : "#333";

    // recordatorios pastillas y comida
    document.getElementById("alertPast").style.display = d.pastillas ? "block" : "none";
    document.getElementById("alertComida").style.display = d.comida ? "block" : "none";

    // añado el último valor a cada gráfica
    charts.gas.data.labels.push(hora);   charts.gas.data.datasets[0].data.push(d.v.at(-1)  || 0);   // v = adc del MQ
    charts.luz.data.labels.push(hora);   charts.luz.data.datasets[0].data.push(d.luz.at(-1) || 0);
    charts.temp.data.labels.push(hora);  charts.temp.data.datasets[0].data.push(d.temp.at(-1)|| 0);

    // mantengo solo los últimos 200 puntos (evita que explote la memoria)
    if (charts.gas.data.labels.length > 200) {
        ["gas","luz","temp"].forEach(t => {
            charts[t].data.labels.shift();
            charts[t].data.datasets[0].data.shift();
        });
    }

    // actualizo gráficas sin animación (más fluido)
    Object.values(charts).forEach(c => c.update('none'));
}, 500);

// ---- ENVÍO DE COMANDOS ----
async function sendAlert(tipo) {                     
    try {
        const resp = await (await fetch(`/${tipo}`)).text();  
        alert(resp);
    } catch(e) {
        console.error("Error enviando comando:", e);
        alert("Fallo al enviar");
    }
}
</script>
</body>
</html>
'''
#  RUTAS DE CONTROL DE LOS BOTONES (ESCRITURA PUERTO SERIE)
@mi_app.route("/ayuda")
def enviar_ayuda():
    puerto_serial.write(b'A')   
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

#  CONTROL VENTILADOR 
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

#  CONTROL LUZ 
@mi_app.route("/luz/off")
def luz_apagar():
    puerto_serial.write(b'N')  
    return "Luz apagada"

@mi_app.route("/luz/medium")
def luz_media():
    puerto_serial.write(b'I')  
    return "Luz media"

@mi_app.route("/luz/high")
def luz_alta():
    puerto_serial.write(b'M') 
    return "Luz alta"

#  CONTROL PERSIANAS
@mi_app.route("/blind/up")
def persiana_subir():
    puerto_serial.write(b'D')
    return "Persiana bajando"

@mi_app.route("/blind/mid")
def persiana_medio():
    puerto_serial.write(b'Z')
    return "Persiana a mitad"

@mi_app.route("/blind/down")
def persiana_bajar():
    puerto_serial.write(b'U.')
    return "Persiana subiendo"

#  RUTA DE DATOS JSON 
@mi_app.route("/data")
def endpoint_datos():
    estado_puerta = "ABIERTA" if puerta == "PA" else "CERRADA"
    estado_persona = "DENTRO" if persona == 1 else "FUERA"
    
    return jsonify({
        "v": gas_adcc.copy(),
        "luz": luz.copy(),
        "temp": temp.copy(),
        "e": estado_actual,
        "ayuda": ayuda,
        "sos": sos,
        "comida": comida,
        "pastillas": pastillas,
        "puerta": estado_puerta,     
        "persona": estado_persona,    
        "puerta_raw": puerta,        
        "persona_raw": persona
    })

#  INICIO DEL HILO Y DEL SERVIDOR 
threading.Thread(target=hilo_lector_tiva, daemon=True).start()
mi_app.run(host="0.0.0.0", port=5000)

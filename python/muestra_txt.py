from flask import Flask
import threading
import time
import os

app = Flask(__name__)

ARCHIVO = r"C:\Users\aledi\Desktop\live_data.txt"
ultimo_texto = ""

def seguir_archivo():
    global ultimo_texto
    while True:
        if os.path.exists(ARCHIVO):
            try:
                with open(ARCHIVO, "r", encoding="utf-8") as f:
                    ultimo_texto = f.read()[-2000:]  # últimas ~2000 caracteres (evita que crezca infinito)
            except:
                pass
        time.sleep(0.5)

@app.route("/")
def index():
    return f"""
    <!DOCTYPE html>
    <html>
    <head>
        <title>Tiva TM4C1294 - Datos en vivo</title>
        <meta charset="utf-8">
        <style>
            body {{ font-family: Consolas, monospace; background: #1e1e1e; color: #d4d4d4; padding: 20px; }}
            pre {{ white-space: pre-wrap; word-wrap: break-word; }}
            .auto {{ position: fixed; bottom: 10px; right: 10px; background: #007acc; color: white; padding: 10px; border-radius: 5px; }}
        </style>
    </head>
    <body>
        <h1>Datos en vivo desde la Tiva TM4C1294</h1>
        <pre id="datos">{ultimo_texto}</pre>
        <div class="auto">Actualizando automáticamente...</div>

        <script>
            setInterval(() => location.reload(), 1000);  // recarga cada segundo
        </script>
    </body>
    </html>
    """

# Hilo que mantiene el archivo leído
threading.Thread(target=seguir_archivo, daemon=True).start()

if __name__ == "__main__":
    print("Abrir en el navegador: http://10.100.46.214:5000")
    app.run(host="0.0.0.0", port=5000)
import paho.mqtt.client as mqtt

umbral_establecido = 50

# Funcion callback invocada cuandl el cliente recibe un CONNACK desde el broker.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Suscribirse en on_connect() asegura que si se pierde la conexión y 
    # se reestablece, las suscripciones se renovarán.
    client.subscribe("EDIFICIO_Pto0903/#")

# Funcion callback al recibir un mensaje de publicacion (PUBLISH) desde el 
# broker.
def on_message(client, userdata, msg):
    if msg.payload > umbral_establecido:
        topic = msg.topic
        value = msg.payload
        mensajes = topic.split("/")
        for mensaje in mensajes:
            print(mensaje + "\n")
        print(value)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect("test.mosquitto.org", 1883, 60)

# Llamada bloqueante que procesa el tráfico de red, invoca callbacks
# y maneja la reconexión al broker. 
client.loop_forever()

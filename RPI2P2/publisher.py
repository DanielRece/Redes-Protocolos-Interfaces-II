import paho.mqtt.publish as publish
from random import randint, uniform
from time import sleep

topics = []
identificadores = ['EDIFICIO_Pto0903']
plantas = range(0,8)
alas = ["N","S","E","W"]
salas = range(0,10)
sensores = ["TEMP", "HUM", "LUX", "VIBR"]

wildcard_e_s = f"{identificadores[0]}/#/TEMP"
wildcard_e_p_a_m =f"{identificadores[0]}/P{plantas[2]}/{alas[3]}/+/VIBR"
wildcard_e_p_a_s = f"{identificadores[0]}/P{plantas[7]}/{alas[1]}/{salas[4]}/#"

for identificador in identificadores:
    for planta in plantas:
        for ala in alas:
            for sala in salas:
                for sensor in sensores:
                    topic = f"{identificador}/P{planta}/{ala}/{sala}/{sensor}"
                    topics.append(topic)




while(1):
    topic = topics[randint(0,len(topics))]
    value = uniform(0, 20)
    sleep(randint(0,5))
    publish.single(topic,value,hostname="test.mosquitto.org")

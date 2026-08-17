# Pompe AquaSolar — DC 3-6V 120L/h

La nouvelle pompe est une mini pompe submersible DC 3-6V, annoncée à 120 L/h.

## Alimentation recommandée

Utiliser une alimentation régulée 5V séparée pour la pompe. 5V est dans la plage 3-6V de la pompe.

**Ne jamais alimenter la pompe directement depuis une GPIO de l'ESP32.**

## Commande recommandée avec MOSFET

```text
Alimentation 5V (+) ───────────────► Pompe (+)
                                      Pompe (-)
                                         │
                                         ▼
                                      Drain MOSFET
ESP32 GPIO23 ── résistance ───────► Gate
ESP32 GND ─────────────────────────► Source MOSFET
Alimentation 5V (-) ───────────────► ESP32 GND

Diode de roue libre sur la pompe :
- cathode (bande) → Pompe (+) / +5V
- anode → Pompe (-) / Drain
```

Utiliser un MOSFET N-channel logic-level adapté au courant de la pompe. Une résistance de grille d'environ 100-220 ohms et une résistance pull-down d'environ 10 kOhm sont recommandées.

## Alternative

Un module MOSFET DC compatible 3.3V peut être utilisé à la place du MOSFET discret. Un relais DC correctement dimensionné est également possible, mais le MOSFET est préférable pour une petite pompe DC.

## Firmware

Le firmware `ranomadio_solar_mqtt.ino` garde GPIO23 comme sortie de commande. La GPIO commande uniquement le MOSFET/transistor/module, jamais le moteur directement.

La télémétrie contient aussi `pumpVoltage: 5.0` pour documenter la configuration de démonstration.

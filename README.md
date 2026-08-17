# Ranomadio Solar

Dashboard Web responsive de surveillance du système de traitement de l'eau.

## Ce qui est inclus
- Dashboard temps réel
- pH, température, niveau du réservoir, batterie
- États rouge / jaune / vert
- Automatisation simulée de la pompe
- Historique et export CSV
- Alertes
- Configuration des seuils pH
- Interface mobile responsive
- Manifest PWA

## Lancer
Ouvrir `index.html` dans un navigateur.

## Important
Cette version est un prototype d'interface avec données simulées. Elle n'est pas encore reliée physiquement à l'ESP32. Pour la version réelle, l'ESP32 devra publier les mesures vers une API sécurisée ou MQTT, et recevoir/assurer la commande de la pompe selon la logique embarquée.

Le CA 10001 visible dans le projet est un pH-mètre portable de référence : il ne constitue pas une entrée pH directe pour l'ESP32. Une sonde/module pH avec sortie électrique compatible ESP32 sera nécessaire pour l'automatisation de la mesure.

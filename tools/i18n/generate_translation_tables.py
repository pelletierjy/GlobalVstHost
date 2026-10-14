#!/usr/bin/env python3
# tools/i18n/generate_translation_tables.py
#
# AUTHORITATIVE SOURCE for the French and Spanish UI translations.
#
# It extracts every key passed to i18n::tr() / i18n::trW() in src/tray-app,
# looks each one up in the FR/ES dictionaries below, and (re)writes the JUCE
# LocalisedStrings tables the app compiles in:
#
#     src/tray-app/i18n/translations_fr.cpp
#     src/tray-app/i18n/translations_es.cpp
#
# Usage:
#     python tools/i18n/generate_translation_tables.py           # regenerate
#     python tools/i18n/generate_translation_tables.py --check   # verify only
#     python tools/i18n/generate_translation_tables.py --list-untranslated
#
# After changing a user-facing string in the C++ sources, re-run this script so
# the tables pick up the new key; commit the regenerated .cpp files with it.
# Keys with no entry fall back to English at runtime (missing key == identity).

import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(WT, "src", "tray-app")

FR_TABLE = os.path.join(SRC, "i18n", "translations_fr.cpp")
ES_TABLE = os.path.join(SRC, "i18n", "translations_es.cpp")

CALL = re.compile(r"\b(?:i18n::)?tr[W]?\s*\(\s*(L?)\"")
LITERAL = re.compile(r"\"((?:[^\"\\]|\\.)*)\"")

# ---------------------------------------------------------------------------
# Translations. Keys are the C++ string literals exactly as written in source
# (escape sequences such as \n stay as-is — LocalisedStrings unescapes them).
# Values that are identical to the English source may be omitted; the runtime
# falls back to the source string.
# ---------------------------------------------------------------------------

FR = {
    "System default": "Langue du système",
    "Ready": "Prêt",
    "In-Process": "Interne",
    "Windows Service": "Service Windows",
    "Unknown": "Inconnu",
    "Engine host mode: ": "Mode hôte du moteur : ",
    "Current output audio device: ": "Périphérique de sortie audio actuel : ",
    "Current captured audio device: ": "Périphérique de capture audio actuel : ",
    "Buffer size: ": "Taille du tampon : ",
    "Sample rate: ": "Fréquence d'échantillonnage : ",
    "Chain revision: ": "Révision de la chaîne : ",
    "Plugins in chain: ": "Greffons dans la chaîne : ",
    "  Resample: ": "  Rééchantillonnage : ",
    "  Chain:    ": "  Chaîne:    ",
    "  Output:   ": "  Sortie:   ",
    "% (peak ": "% (pic ",
    "Xruns this session: ": "Xruns cette session : ",
    "Toggle energy saver: auto-suspend the VST chain when input is silent to save CPU.":
        "Économiseur d'énergie : suspendre automatiquement la chaîne VST quand l'entrée est silencieuse pour économiser le CPU.",
    r"Latency (ms):\n": r"Latence (ms):\n",
    "About": "À propos",
    "Settings": "Paramètres",
    "About / Diagnostics": "À propos / Diagnostics",
    "Select Plugin": "Choisir un greffon",
    "Add": "Ajouter",
    "Browse...": "Parcourir...",
    "Cancel": "Annuler",
    " [Built-in]": " [Intégré]",
    "Placeholder": "Emplacement vide",
    "Clear tag": "Effacer l'étiquette",
    "Add a tag": "Ajouter une étiquette",
    "Remove this plugin's tray shortcut": "Retirer le raccourci du menu système de ce greffon",
    "Two tray shortcuts are already assigned": "Deux raccourcis du menu système sont déjà assignés",
    "Give this plugin a tray shortcut button": "Attribuer un bouton raccourci du menu système à ce greffon",
    "Input": "Entrée",
    "Output": "Sortie",
    "Master Volume": "Volume principal",
    "System: ": "Système : ",
    "Input: ": "Entrée : ",
    "AUDIO DEVICE": "PÉRIPHÉRIQUE AUDIO",
    "LEVELS": "NIVEAUX",
    "PLUGIN CHAIN": "CHAÎNE DE GREFFONS",
    "SPECTRUM": "SPECTRE",
    "Energy Saver: off": "Économiseur d'énergie : désactivé",
    "Energy Saver: sleeping (engine idle)": "Économiseur d'énergie : en veille (moteur inactif)",
    "Energy Saver: on (monitoring)": "Économiseur d'énergie : activé (surveillance)",
    "Start/Stop audio": "Démarrer/arrêter l'audio",
    "Reset engine": "Réinitialiser le moteur",
    "Scanning plugins...": "Analyse des greffons...",
    "Start minimized to tray": "Démarrer réduit dans le menu système",
    "Show Global VST Host": "Afficher Global VST Host",
    "Stop Audio": "Arrêter l'audio",
    "Start Audio": "Démarrer l'audio",
    "Exit": "Quitter",
    "Mode:": "Mode :",
    "Captured audio device:": "Périphérique de capture :",
    "Output audio device:": "Périphérique de sortie :",
    "ASIO Device:": "Périphérique ASIO :",
    "Channels:": "Canaux :",
    "Buffer:": "Tampon :",
    "Captured sampling rate:": "Fréquence capturée :",
    "Output sampling rate:": "Fréquence de sortie :",
    "VST sampling rate:": "Fréquence VST :",
    "Vol:": "Vol :",
    "ASIO Settings...": "Paramètres ASIO...",
    "Save Preset...": "Enregistrer le préréglage...",
    "Load Preset...": "Charger un préréglage...",
    "Latency: \u2014 ms": "Latence : \u2014 ms",
    "Out": "Sortie",
    "Theme:": "Thème :",
    "Light": "Clair",
    "Start minimized:": "Démarrer réduit :",
    "to tray": "réduit",
    "Tooltips:": "Info-bulles :",
    "enabled": "activées",
    "Language:": "Langue :",
    "Disconnected": "Déconnecté",
    "Engine reset": "Moteur réinitialisé",
    "Audio running": "Audio en cours",
    "Audio stopped": "Audio arrêté",
    "Device Conflict Resolved": "Conflit de périphérique résolu",
    r"WASAPI loopback requires two separate audio interfaces:\n"
    r"one for capture (system audio) and one for playback (processed output).\n\n"
    r"The output device has been disconnected. The input will be muted automatically.\n\n"
    r"Please connect a separate playback device such as USB headphones, an external display with audio output, a DAC, or similar.\n\n"
    r"Alternatively, you can install a virtual audio cable such as VB-Cable from https://vb-audio.com/Cable/ ":
        r"WASAPI loopback nécessite deux interfaces audio distinctes :\n"
        r"une pour la capture (audio système) et une pour la lecture (sortie traitée).\n\n"
        r"Le périphérique de sortie a été déconnecté. L'entrée sera mise en sourdine automatiquement.\n\n"
        r"Connectez un périphérique de lecture distinct, comme des écouteurs USB, un écran avec sortie audio, un DAC ou l'équivalent.\n\n"
        r"Vous pouvez aussi installer un câble audio virtuel comme VB-Cable depuis https://vb-audio.com/Cable/ ",
    r"WASAPI loopback requires two separate audio interfaces:\n"
    r"one for capture (system audio) and one for playback (processed output).\n\n"
    r"The input device has been disconnected. It will be muted automatically.\n\n"
    r"Please connect a separate playback device such as USB headphones, an external display with audio output, a DAC, or similar.\n\n"
    r"Alternatively, you can install a virtual audio cable such as VB-Cable from https://vb-audio.com/Cable/ ":
        r"WASAPI loopback nécessite deux interfaces audio distinctes :\n"
        r"une pour la capture (audio système) et une pour la lecture (sortie traitée).\n\n"
        r"L'entrée a été déconnectée. Elle sera mise en sourdine automatiquement.\n\n"
        r"Connectez un périphérique de lecture distinct, comme des écouteurs USB, un écran avec sortie audio, un DAC ou l'équivalent.\n\n"
        r"Vous pouvez aussi installer un câble audio virtuel comme VB-Cable depuis https://vb-audio.com/Cable/ ",
    "Buffer Size Not Supported": "Taille de tampon non prise en charge",
    "Language saved \u2014 restart Global VST Host to apply":
        "Langue enregistrée \u2014 redémarrez Global VST Host pour l'appliquer",
    "Select audio transport: WASAPI (loopback) or ASIO":
        "Sélectionner le transport audio : WASAPI (boucle de retour) ou ASIO",
    "Capture source: system audio loopback or a specific capture device":
        "Source de capture : boucle de retour du système ou périphérique précis",
    "Playback device: where processed audio is sent":
        "Périphérique de lecture : où l'audio traité est envoyé",
    "ASIO hardware driver to use": "Pilote matériel ASIO à utiliser",
    "Stereo output channel pair": "Paire de canaux de sortie stéréo",
    "Audio buffer size in samples (larger = more stable, smaller = lower latency)":
        "Taille du tampon audio en échantillons (plus grand = plus stable, plus petit = latence plus faible)",
    "Target sampling rate for the VST plugin chain":
        "Fréquence d'échantillonnage cible de la chaîne de greffons VST",
    "Start or stop audio processing": "Démarrer ou arrêter le traitement audio",
    "Master output volume": "Volume de sortie principal",
    "Mute / unmute the output": "Couper / rétablir la sortie",
    "Reset the audio engine and reload the plugin chain":
        "Réinitialiser le moteur audio et recharger la chaîne de greffons",
    "Save the current plugin chain and settings to a file":
        "Enregistrer la chaîne de greffons et les paramètres dans un fichier",
    "Load a previously saved preset file": "Charger un préréglage enregistré",
    "Open the ASIO driver control panel": "Ouvrir le panneau de contrôle du pilote ASIO",
    "Choose the application colour theme: light, dark, or match the system setting.":
        "Choisir le thème de couleur : clair, sombre ou aligné sur le système.",
    "Interface language: takes effect the next time you start Global VST Host.":
        "Langue de l'interface : prend effet au prochain démarrage de Global VST Host.",
    "When enabled, the app starts directly in the system tray without showing the main window.":
        "Si activé, l'application démarre directement dans le menu système sans afficher la fenêtre principale.",
    "Show helpful popup descriptions when hovering over controls. Turn this off for a cleaner interface.":
        "Afficher des descriptions utiles au survol des contrôles. Désactivez pour une interface plus épurée.",
    "About, diagnostics, and settings": "À propos, diagnostics et paramètres",
    "Open help documentation": "Ouvrir la documentation d'aide",
    "Plugin added: ": "Greffon ajouté : ",
    "VST3 Plugins (*.vst3)": "Greffons VST3 (*.vst3)",
    "All Files": "Tous les fichiers",
    "Global VST Host Presets (*.jvst)": "Préréglages de Global VST Host (*.jvst)",
    "Preset saved: ": "Préréglage enregistré : ",
    "Preset loaded: ": "Préréglage chargé : ",
    "Energy Saver enabled": "Économiseur d'énergie activé",
    "Energy Saver disabled": "Économiseur d'énergie désactivé",
    "Energy Saver: engine sleeping (no audio)": "Économiseur d'énergie : moteur en veille (pas d'audio)",
    "Energy Saver: audio resumed": "Économiseur d'énergie : audio repris",
    "Session restored - starting audio...": "Session restaurée - démarrage de l'audio...",
    "Session restored - audio running": "Session restaurée - audio en cours",
    "Session restored - failed to start audio": "Session restaurée - échec du démarrage de l'audio",
    "Session restored - output device not found": "Session restaurée - périphérique de sortie introuvable",
    "Session restored": "Session restaurée",
    "Latency: %.2f ms (in %.2f + out %.2f)": "Latence : %.2f ms (entrée %.2f + sortie %.2f)",
    "Chain updated (rev ": "Chaîne mise à jour (rév. ",
    "Plugin failed: ": "Échec du greffon : ",
    "Device lost: ": "Périphérique perdu : ",
    " \u2192 fallback: ": " \u2192 secours : ",
    "Device restored: ": "Périphérique restauré : ",
    "CPU warning: %.1f %% \u2014 consider larger buffer":
        "Avertissement CPU : %.1f %% \u2014 envisagez un tampon plus grand",
    "Preset loaded with missing plugins: ": "Préréglage chargé avec des greffons manquants : ",
    "Cannot use same device for capture and output":
        "Impossible d'utiliser le même périphérique pour la capture et la sortie",
    "Capture device muting unavailable; select a different output device":
        "Mise en sourdine du périphérique de capture indisponible ; choisissez un autre périphérique de sortie",
    "Audio stopped (system sleep)": "Audio arrêté (veille du système)",
    "System resumed; restarting audio...": "Système repris ; redémarrage de l'audio...",
    "Audio running (system resume)": "Audio en cours (reprise du système)",
    "Audio device is not responding; reselect a device or press Reset":
        "Le périphérique audio ne répond pas ; resélectionnez-en un ou appuyez sur Réinitialiser",
    "Audio stalled; restarting engine (attempt %d)":
        "Audio bloqué ; redémarrage du moteur (tentative %d)",
    " \u2014 Editor": " \u2014 Éditeur",
    "Scanning VST3 Plugins": "Analyse des greffons VST3",
    "Starting scan...": "Démarrage de l'analyse...",
    "Plugins found: 0": "Greffons trouvés : 0",
    "Scanning...": "Analyse...",
    "Scanning: ": "Analyse : ",
    "Scan complete.": "Analyse terminée.",
    "Scan cancelled.": "Analyse annulée.",
    "Close": "Fermer",
    "Plugins found: ": "Greffons trouvés : ",
    "Add...": "Ajouter...",
    "Remove": "Retirer",
    "Select VST3 folder": "Choisir le dossier VST3",
    "Spectrum": "Spectre",
}

ES = {
    "System default": "Idioma del sistema",
    "Ready": "Listo",
    "In-Process": "Interno",
    "Windows Service": "Servicio de Windows",
    "Unknown": "Desconocido",
    "Engine host mode: ": "Modo de host del motor: ",
    "Current output audio device: ": "Dispositivo de salida de audio actual: ",
    "Current captured audio device: ": "Dispositivo de captura de audio actual: ",
    "Buffer size: ": "Tamaño del búfer: ",
    "Sample rate: ": "Frecuencia de muestreo: ",
    "Chain revision: ": "Revisión de la cadena: ",
    "Plugins in chain: ": "Plugins en la cadena: ",
    "  Resample: ": "  Remuestreo: ",
    "  Chain:    ": "  Cadena:    ",
    "  Output:   ": "  Salida:   ",
    "% (peak ": "% (pico ",
    "Xruns this session: ": "Xruns en esta sesión: ",
    "Toggle energy saver: auto-suspend the VST chain when input is silent to save CPU.":
        "Ahorrador de energía: suspende la cadena VST automáticamente cuando la entrada está en silencio para ahorrar CPU.",
    r"Latency (ms):\n": r"Latencia (ms):\n",
    "About": "Acerca de",
    "Settings": "Configuración",
    "About / Diagnostics": "Acerca de / Diagnóstico",
    "Select Plugin": "Seleccionar plugin",
    "Add": "Añadir",
    "Browse...": "Examinar...",
    "Cancel": "Cancelar",
    " [Built-in]": " [Integrado]",
    "Placeholder": "Espacio libre",
    "Clear tag": "Borrar etiqueta",
    "Add a tag": "Añadir etiqueta",
    "Remove this plugin's tray shortcut": "Quitar el acceso directo de la bandeja de este plugin",
    "Two tray shortcuts are already assigned": "Ya se asignaron dos accesos directos de la bandeja",
    "Give this plugin a tray shortcut button": "Asignar un botón de acceso directo de la bandeja a este plugin",
    "Input": "Entrada",
    "Output": "Salida",
    "Master Volume": "Volumen principal",
    "System: ": "Sistema: ",
    "Input: ": "Entrada: ",
    "AUDIO DEVICE": "DISPOSITIVO DE AUDIO",
    "LEVELS": "NIVELES",
    "PLUGIN CHAIN": "CADENA DE PLUGINS",
    "SPECTRUM": "ESPECTRO",
    "Energy Saver: off": "Ahorrador de energía: desactivado",
    "Energy Saver: sleeping (engine idle)": "Ahorrador de energía: en suspensión (motor inactivo)",
    "Energy Saver: on (monitoring)": "Ahorrador de energía: activado (monitoreo)",
    "Start/Stop audio": "Iniciar/detener el audio",
    "Reset engine": "Reiniciar el motor",
    "Scanning plugins...": "Analizando plugins...",
    "Start minimized to tray": "Iniciar minimizado en la bandeja",
    "Show Global VST Host": "Mostrar Global VST Host",
    "Stop Audio": "Detener el audio",
    "Start Audio": "Iniciar el audio",
    "Exit": "Salir",
    "Mode:": "Modo:",
    "Captured audio device:": "Dispositivo de captura:",
    "Output audio device:": "Dispositivo de salida:",
    "ASIO Device:": "Dispositivo ASIO:",
    "Channels:": "Canales:",
    "Buffer:": "Búfer:",
    "Captured sampling rate:": "Frecuencia de captura:",
    "Output sampling rate:": "Frecuencia de salida:",
    "VST sampling rate:": "Frecuencia VST:",
    "Vol:": "Vol:",
    "ASIO Settings...": "Configuración de ASIO...",
    "Save Preset...": "Guardar preconfiguración...",
    "Load Preset...": "Cargar preconfiguración...",
    "Latency: \u2014 ms": "Latencia: \u2014 ms",
    "Out": "Salida",
    "Theme:": "Tema:",
    "Light": "Claro",
    "Start minimized:": "Iniciar minimizado:",
    "to tray": "minimizado",
    "Tooltips:": "Descripciones:",
    "enabled": "activadas",
    "Language:": "Idioma:",
    "Disconnected": "Desconectado",
    "Engine reset": "Motor reiniciado",
    "Audio running": "Audio en ejecución",
    "Audio stopped": "Audio detenido",
    "Device Conflict Resolved": "Conflicto de dispositivo resuelto",
    r"WASAPI loopback requires two separate audio interfaces:\n"
    r"one for capture (system audio) and one for playback (processed output).\n\n"
    r"The output device has been disconnected. The input will be muted automatically.\n\n"
    r"Please connect a separate playback device such as USB headphones, an external display with audio output, a DAC, or similar.\n\n"
    r"Alternatively, you can install a virtual audio cable such as VB-Cable from https://vb-audio.com/Cable/ ":
        r"El loopback de WASAPI requiere dos interfaces de audio separadas:\n"
        r"una para la captura (audio del sistema) y otra para la reproducción (salida procesada).\n\n"
        r"El dispositivo de salida se ha desconectado. La entrada se silenciará automáticamente.\n\n"
        r"Conecte un dispositivo de reproducción separado, como auriculares USB, una pantalla con salida de audio, un DAC o similar.\n\n"
        r"También puede instalar un cable de audio virtual como VB-Cable desde https://vb-audio.com/Cable/ ",
    r"WASAPI loopback requires two separate audio interfaces:\n"
    r"one for capture (system audio) and one for playback (processed output).\n\n"
    r"The input device has been disconnected. It will be muted automatically.\n\n"
    r"Please connect a separate playback device such as USB headphones, an external display with audio output, a DAC, or similar.\n\n"
    r"Alternatively, you can install a virtual audio cable such as VB-Cable from https://vb-audio.com/Cable/ ":
        r"El loopback de WASAPI requiere dos interfaces de audio separadas:\n"
        r"una para la captura (audio del sistema) y otra para la reproducción (salida procesada).\n\n"
        r"La entrada se ha desconectado. Se silenciará automáticamente.\n\n"
        r"Conecte un dispositivo de reproducción separado, como auriculares USB, una pantalla con salida de audio, un DAC o similar.\n\n"
        r"También puede instalar un cable de audio virtual como VB-Cable desde https://vb-audio.com/Cable/ ",
    "Buffer Size Not Supported": "Tamaño de búfer no admitido",
    "Language saved \u2014 restart Global VST Host to apply":
        "Idioma guardado \u2014 reinicie Global VST Host para aplicarlo",
    "Select audio transport: WASAPI (loopback) or ASIO":
        "Seleccionar transporte de audio: WASAPI (loopback) o ASIO",
    "Capture source: system audio loopback or a specific capture device":
        "Fuente de captura: loopback del audio del sistema o un dispositivo específico",
    "Playback device: where processed audio is sent":
        "Dispositivo de reproducción: a donde se envía el audio procesado",
    "ASIO hardware driver to use": "Controlador de hardware ASIO a usar",
    "Stereo output channel pair": "Par de canales de salida estéreo",
    "Audio buffer size in samples (larger = more stable, smaller = lower latency)":
        "Tamaño del búfer de audio en muestras (mayor = más estable, menor = menor latencia)",
    "Target sampling rate for the VST plugin chain":
        "Frecuencia de muestreo objetivo de la cadena de plugins VST",
    "Start or stop audio processing": "Iniciar o detener el procesamiento de audio",
    "Master output volume": "Volumen de salida principal",
    "Mute / unmute the output": "Silenciar / activar la salida",
    "Reset the audio engine and reload the plugin chain":
        "Reiniciar el motor de audio y recargar la cadena de plugins",
    "Save the current plugin chain and settings to a file":
        "Guardar la cadena de plugins y la configuración en un archivo",
    "Load a previously saved preset file": "Cargar una preconfiguración guardada",
    "Open the ASIO driver control panel": "Abrir el panel de control del controlador ASIO",
    "Choose the application colour theme: light, dark, or match the system setting.":
        "Elegir el tema de color: claro, oscuro o igual que el sistema.",
    "Interface language: takes effect the next time you start Global VST Host.":
        "Idioma de la interfaz: se aplica la próxima vez que inicie Global VST Host.",
    "When enabled, the app starts directly in the system tray without showing the main window.":
        "Si está activado, la aplicación inicia directamente en la bandeja sin mostrar la ventana principal.",
    "Show helpful popup descriptions when hovering over controls. Turn this off for a cleaner interface.":
        "Mostrar descripciones útiles al pasar el cursor sobre los controles. Desactívelo para una interfaz más limpia.",
    "About, diagnostics, and settings": "Acerca de, diagnóstico y configuración",
    "Open help documentation": "Abrir la documentación de ayuda",
    "Plugin added: ": "Plugin añadido: ",
    "VST3 Plugins (*.vst3)": "Plugins VST3 (*.vst3)",
    "All Files": "Todos los archivos",
    "Global VST Host Presets (*.jvst)": "Preconfiguraciones de Global VST Host (*.jvst)",
    "Preset saved: ": "Preconfiguración guardada: ",
    "Preset loaded: ": "Preconfiguración cargada: ",
    "Energy Saver enabled": "Ahorrador de energía activado",
    "Energy Saver disabled": "Ahorrador de energía desactivado",
    "Energy Saver: engine sleeping (no audio)": "Ahorrador de energía: motor en suspensión (sin audio)",
    "Energy Saver: audio resumed": "Ahorrador de energía: audio reanudado",
    "Session restored - starting audio...": "Sesión restaurada - iniciando el audio...",
    "Session restored - audio running": "Sesión restaurada - audio en ejecución",
    "Session restored - failed to start audio": "Sesión restaurada - error al iniciar el audio",
    "Session restored - output device not found": "Sesión restaurada - dispositivo de salida no encontrado",
    "Session restored": "Sesión restaurada",
    "Latency: %.2f ms (in %.2f + out %.2f)": "Latencia: %.2f ms (entrada %.2f + salida %.2f)",
    "Chain updated (rev ": "Cadena actualizada (rev. ",
    "Plugin failed: ": "Error del plugin: ",
    "Device lost: ": "Dispositivo perdido: ",
    " \u2192 fallback: ": " \u2192 alternativa: ",
    "Device restored: ": "Dispositivo restaurado: ",
    "CPU warning: %.1f %% \u2014 consider larger buffer":
        "Advertencia de CPU: %.1f %% \u2014 considere un búfer más grande",
    "Preset loaded with missing plugins: ": "Preconfiguración cargada con plugins faltantes: ",
    "Cannot use same device for capture and output":
        "No se puede usar el mismo dispositivo para captura y salida",
    "Capture device muting unavailable; select a different output device":
        "No se puede silenciar el dispositivo de captura; seleccione otro dispositivo de salida",
    "Audio stopped (system sleep)": "Audio detenido (suspensión del sistema)",
    "System resumed; restarting audio...": "Sistema reanudado; reiniciando el audio...",
    "Audio running (system resume)": "Audio en ejecución (reanudación del sistema)",
    "Audio device is not responding; reselect a device or press Reset":
        "El dispositivo de audio no responde; vuelva a seleccionarlo o pulse Reiniciar",
    "Audio stalled; restarting engine (attempt %d)":
        "Audio bloqueado; reiniciando el motor (intento %d)",
    "Scanning VST3 Plugins": "Analizando plugins VST3",
    "Starting scan...": "Iniciando el análisis...",
    "Plugins found: 0": "Plugins encontrados: 0",
    "Scanning...": "Analizando...",
    "Scanning: ": "Analizando: ",
    "Scan complete.": "Análisis completado.",
    "Scan cancelled.": "Análisis cancelado.",
    "Close": "Cerrar",
    "Plugins found: ": "Plugins encontrados: ",
    "Add...": "Añadir...",
    "Remove": "Quitar",
    "Select VST3 folder": "Seleccionar la carpeta VST3",
    "Spectrum": "Espectro",
}

HEADER = """// src/tray-app/i18n/translations_{code}.cpp
//
// {language} translation table for the tray app.
//
// GENERATED FILE — do not edit by hand. The authoritative translations live in
// tools/i18n/generate_translation_tables.py; re-run that script after changing
// a user-facing string (or a translation) and commit the result:
//
//     python tools/i18n/generate_translation_tables.py
//
// Format: JUCE LocalisedStrings text — one "English source" = "{language}"
// pair per line. Keys are exactly the literals passed to i18n::tr()/i18n::trW()
// in the sources; escape sequences (\\n, \\", \\\\) are interpreted by
// LocalisedStrings, so they are written the same way as in the C++ source.
// A key with no entry falls back to English at runtime.

namespace jyglobalvst::tray::i18n::tables
{{
namespace
{{
// i18n-OK: the {language} table is the data half of the sanctioned FR/ES
// localization mechanism (see i18n/translation.cpp).
constexpr const char* kTable = R"jyv(
language: {language}
countries: {countries}

{entries}
)jyv";
}}  // namespace

const char* {func}()
{{
    return kTable;
}}

}}  // namespace jyglobalvst::tray::i18n::tables
"""


def extract_keys():
    keys = []
    for root, _dirs, files in os.walk(SRC):
        for name in sorted(files):
            if not name.endswith((".cpp", ".h")):
                continue
            path = os.path.join(root, name)
            with io.open(path, "r", encoding="utf-8") as fh:
                text = fh.read()
            for m in CALL.finditer(text):
                pos = m.end() - 1
                parts = []
                while pos < len(text):
                    if text[pos] == "L":
                        pos += 1
                        continue
                    if text[pos] != '"':
                        break
                    mm = LITERAL.match(text, pos)
                    if not mm:
                        break
                    parts.append(mm.group(1))
                    pos = mm.end()
                    nxt = re.compile(r"\s*").match(text, pos).end()
                    if nxt < len(text) and (text[nxt] == '"' or text[nxt] == "L"):
                        pos = nxt
                    else:
                        break
                key = "".join(parts)
                if key and key not in keys:
                    keys.append(key)
    return keys


def check_safety(entries):
    # A key that contains a double quote cannot be represented in the table
    # format (LocalisedStrings stops at the first unescaped quote).
    for key, value in entries:
        for bad in (key, value):
            if '"' in bad:
                raise SystemExit("FATAL: embedded double quote in key/value: %r" % bad[:80])


def render(code, language, countries, func, mapping, keys):
    rows = []
    untranslated = []
    for key in keys:
        value = mapping.get(key)
        if value is None:
            untranslated.append(key)
            continue
        check_safety([(key, value)])
        rows.append('"%s" = "%s"' % (key, value))
    body = HEADER.format(
        code=code,
        language=language,
        countries=countries,
        func=func,
        entries="\n".join(rows),
    )
    return body, untranslated


def write_if_changed(path, content):
    if os.path.exists(path):
        with io.open(path, "r", encoding="utf-8") as fh:
            if fh.read() == content:
                return False
    with io.open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(content)
    return True


def main():
    check = "--check" in sys.argv
    keys = extract_keys()

    fr_body, fr_missing = render("fr", "French", "fr be mc ch lu", "french", FR, keys)
    es_body, es_missing = render("es", "Spanish", "es mx ar co cl pe", "spanish", ES, keys)

    status = 0
    for path, body in ((FR_TABLE, fr_body), (ES_TABLE, es_body)):
        if check:
            with io.open(path, "r", encoding="utf-8") as fh:
                if fh.read() != body:
                    print("STALE: %s (re-run generate_translation_tables.py)" % path)
                    status = 1
        else:
            if write_if_changed(path, body):
                print("wrote %s" % path)

    print("keys found in sources: %d" % len(keys))
    if fr_missing:
        print("keys with no French entry (fall back to English): %d" % len(fr_missing))
        for k in fr_missing:
            print("  FR: %r" % k)
    if es_missing:
        print("keys with no Spanish entry (fall back to English): %d" % len(es_missing))
        for k in es_missing:
            print("  ES: %r" % k)

    unknown = [k for k in list(FR) + list(ES) if k not in keys]
    if unknown:
        print("STALE DICT ENTRIES (key no longer used in sources): %d" % len(set(unknown)))
        for k in sorted(set(unknown)):
            print("  %r" % k)
        if check:
            status = 1

    return status


if __name__ == "__main__":
    sys.exit(main())

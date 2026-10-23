@echo off
rem Prende JARVIS en la compu: el tunel de ngrok (para usarlo desde cualquier red, tambien con los
rem datos del celular) y n8n con los datos de este proyecto (flujos y claves en n8n-servidor).
rem Doble clic y dejar las dos ventanas abiertas. Editor de n8n: http://localhost:5678
rem Usa la ruta completa del proyecto: se puede copiar o mover a cualquier lado (p. ej. el escritorio).
rem NODES_EXCLUDE: habilita el nodo "Execute Command" (la voz); n8n 2 lo trae apagado.
set PROYECTO=D:\Users\Lolo\Documents\PlatformIO\Projects\proyecto-jervis
if not exist "%PROYECTO%\node_modules\.bin\n8n.cmd" (
    echo No encuentro n8n en %PROYECTO%. Si moviste el proyecto, cambia PROYECTO en este archivo.
    pause
    exit /b 1
)
cd /d "%PROYECTO%"
rem El tunel se abre solo si no esta abierto ya (asi se puede reabrir solo n8n).
tasklist /fi "imagename eq ngrok.exe" | find /i "ngrok.exe" >nul || start "ngrok - JARVIS" ngrok http 5678 --url=https://excretory-copier-cottage.ngrok-free.dev
set N8N_USER_FOLDER=%PROYECTO%\n8n-servidor\.n8n
set NODES_EXCLUDE=["n8n-nodes-base.localFileTrigger"]
rem Que no consulte internet al arrancar (novedades, plantillas, estadisticas): arranca mas rapido.
set N8N_DIAGNOSTICS_ENABLED=false
set N8N_VERSION_NOTIFICATIONS_ENABLED=false
set N8N_TEMPLATES_ENABLED=false
set N8N_HIRING_BANNER_ENABLED=false
set N8N_RUNNERS_ENABLED=true
set TEMP=D:\temp
set TMP=D:\temp
title n8n - JARVIS
call node_modules\.bin\n8n.cmd start
pause

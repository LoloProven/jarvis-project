@echo off
rem Prende JARVIS en la compu: el tunel de ngrok (para usarlo desde cualquier red, tambien con los
rem datos del celular) y n8n con los datos de este proyecto (flujos y claves en n8n-servidor).
rem Doble clic y dejar las dos ventanas abiertas. Editor de n8n: http://localhost:5678
rem NODES_EXCLUDE: habilita el nodo "Execute Command" (la voz); n8n 2 lo trae apagado.
cd /d "%~dp0.."
start "ngrok - JARVIS" ngrok http 5678 --url=https://excretory-copier-cottage.ngrok-free.dev
set N8N_USER_FOLDER=%CD%\n8n-servidor\.n8n
set NODES_EXCLUDE=["n8n-nodes-base.localFileTrigger"]
set TEMP=D:\temp
set TMP=D:\temp
call node_modules\.bin\n8n.cmd start
pause

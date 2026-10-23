@echo off
rem Prende n8n con los datos de este proyecto (flujos y claves en n8n-servidor).
rem Doble clic para usar JARVIS desde el reloj; dejar esta ventana abierta. Editor: http://localhost:5678
rem NODES_EXCLUDE: habilita el nodo "Execute Command" (voz de Windows de respaldo); n8n 2 lo trae apagado.
cd /d "%~dp0.."
set N8N_USER_FOLDER=%CD%\n8n-servidor\.n8n
set NODES_EXCLUDE=["n8n-nodes-base.localFileTrigger"]
set TEMP=D:\temp
set TMP=D:\temp
call node_modules\.bin\n8n.cmd start
pause

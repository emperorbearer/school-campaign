# run.ps1
# Script to launch both Django Backend and Svelte Frontend

$ScriptPath = $MyInvocation.MyCommand.Path
$BaseDir = Split-Path -Path $ScriptPath -Parent

Write-Host "Starting Django Backend..." -ForegroundColor Cyan
Start-Process powershell -ArgumentList "-NoExit -Command `"cd '$BaseDir\backend'; .\venv\Scripts\activate; python manage.py runserver`""

Write-Host "Starting Svelte Frontend..." -ForegroundColor Cyan
Start-Process powershell -ArgumentList "-NoExit -Command `"cd '$BaseDir\frontend'; npm run dev -- --host`""

Write-Host "Both servers are starting in separate windows." -ForegroundColor Green
Write-Host "Frontend will be available at http://localhost:5173" -ForegroundColor Green
Write-Host "Django Admin is at http://127.0.0.1:8000/admin (User: admin, Pass: admin)" -ForegroundColor Green

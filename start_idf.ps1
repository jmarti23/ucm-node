# UCM ESP-IDF v5.5.5 environment

# Activate ESP-IDF Python environment
& "C:\Espressif\tools\python\v5.5.5\venv\Scripts\Activate.ps1"

# ESP-IDF configuration
$env:IDF_PATH = "C:\esp\v5.5.5\esp-idf"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v5.5.5\venv"
$env:ESP_IDF_VERSION = "5.5.5"

# Load ESP-IDF toolchain
& "$env:IDF_PATH\export.ps1"

# Go to UCM project
Set-Location C:\esp\projects\wifi_test
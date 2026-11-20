# Run from anywhere: .\tools\fw.ps1 deploy s3
$taskPython = if ($env:BLDSP_IDF_PYTHON) { $env:BLDSP_IDF_PYTHON } else { Join-Path $env:USERPROFILE '.espressif\python_env\idf5.4_py3.11_env\Scripts\python.exe' }
& $taskPython (Join-Path $PSScriptRoot 'fw.py') @args
exit $LASTEXITCODE

@echo off
setlocal
pushd "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main protocol_test.c ..\main\protocol.c /Fe:protocol_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
protocol_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main source_policy_test.c ..\main\source_policy.c /Fe:source_policy_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
source_policy_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main status_visual_test.c ..\main\status_visual.c /Fe:status_visual_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
status_visual_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main telemetry_math_test.c ..\main\telemetry_math.c /Fe:telemetry_math_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
telemetry_math_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main monitor_log_test.c ..\main\monitor_log.c /Fe:monitor_log_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
monitor_log_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main thermal_model_test.c ..\main\thermal_model.c /Fe:thermal_model_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
thermal_model_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main thermal_observer_test.c ..\main\thermal_observer.c /Fe:thermal_observer_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
thermal_observer_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main clip_detector_test.c ..\main\clip_detector.c /Fe:clip_detector_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
clip_detector_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main protection_policy_test.c ..\main\protection_policy.c /Fe:protection_policy_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
protection_policy_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main clip_health_test.c ..\main\clip_health.c /Fe:clip_health_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
clip_health_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main clip_alert_test.c ..\main\clip_alert.c /Fe:clip_alert_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
clip_alert_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main knob_policy_test.c ..\main\knob_policy.c /Fe:knob_policy_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
knob_policy_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\shared ws2812_test.c /Fe:ws2812_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
ws2812_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\shared panel_ota_test.c /Fe:panel_ota_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
panel_ota_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\shared strip_motion_test.c ..\shared\strip_motion.c /Fe:strip_motion_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
strip_motion_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\shared strip_visual_test.c ..\shared\strip_visual.c ..\shared\strip_motion.c /Fe:strip_visual_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
strip_visual_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
cl /nologo /std:c11 /W4 /I..\main light_policy_test.c ..\main\light_policy.c /Fe:light_policy_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
light_policy_test.exe
if not "%errorlevel%"=="0" exit /b %errorlevel%
node panel_ota_ui_test.js
if not "%errorlevel%"=="0" exit /b %errorlevel%
node ui_test.js
if not "%errorlevel%"=="0" exit /b %errorlevel%
node response_test.js
if not "%errorlevel%"=="0" exit /b %errorlevel%
popd
